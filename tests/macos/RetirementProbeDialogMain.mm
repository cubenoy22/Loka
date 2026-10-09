#include <AppKit/AppKit.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "RetirementProbeLog.hpp"
#include "RetirementProbeBitmap.hpp"
#include "ScenarioWindow.hpp"
#include "MainNode.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "core/util/ScopedPtr.hpp"
#include "core/Operation.hpp"
#include "testing/core/StateTrackerTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "platform/file/FileIO.hpp"

// Existing rail friend; private access remains confined to this test executable.
class MacDialogResultTestAccess
{
public:
  static void synchronous(MacOpenFileDialogContext &context)
  {
    id presenter = (id)context.deferredPresenter_;
    [presenter performSelector:@selector(cancelPresent)];
    context.disposeDialog();
    [presenter performSelector:@selector(detachOwner)];
    [presenter release];
    context.deferredPresenter_ = 0;
    context.presentation_.markDetached();
    context.presentIfNeeded(); // The existing nil-presenter fallback.
  }
  static bool returnFile(MacOpenFileDialogContext &context, const loka::file::File &file)
  {
    if (!context.registration_) return false;
    [(id)context.deferredPresenter_ performSelector:@selector(cancelPresent)];
    context.presentation_.markPresented();
    loka::app::DialogResultTransport::ReturnPort port(context.registration_);
    return port.seal(loka::app::FileChooserResult::File(file)) != 0;
  }
};
namespace simpleviewer { namespace testing {
class MainAccess
{
public:
  static void dimensions(const MainNode &node, int &width, int &height)
  {
    const loka::core::resource::Image &image = node.image_.state()->getRef();
    width = image.width(); height = image.height();
  }
};
} }

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using loka::core::resource::Image;
  OpenFileDialogNode *FindDialog(Node *node)
  {
    if (!node) return 0;
    if (node->asOpenFileDialogNode()) return node->asOpenFileDialogNode();
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
      if (OpenFileDialogNode *found = FindDialog(child)) return found;
    return 0;
  }
  // Each arm is a fresh generation, so the sync override runs exactly once.
  class DialogRoot;
  typedef BoundaryPropsFor<DialogRoot> DialogProps;
  class DialogRoot : public StdCompositionBoundaryNodeBase<DialogProps>
  {
    typedef StdCompositionBoundaryNodeBase<DialogProps> Base;
  public:
    explicit DialogRoot(const DialogProps &props) : Base(props), sync_(false)
    {
      this->state(this->shown[0], false);
      this->state(this->shown[1], false);
      this->state(this->result, FileChooserResult());
    }
    virtual void composeNode(NodeComposition &composition)
    {
      for (int purpose = 0; purpose != 2; ++purpose)
      {
        OpenFileDialogProps props;
        props.options_ = FileDialogOptions(purpose ? FILE_DIALOG_SAVE : FILE_DIALOG_OPEN);
        composition.declare(Show(*this->shown[purpose].state()).destroyOnDetach()
            << OpenFileDialog(props).result(this->result));
      }
    }
    virtual void applyPendingUpdate(const PlatformApplyPlan &plan)
    {
      Base::applyPendingUpdate(plan);
      OpenFileDialogNode *dialog = FindDialog(this);
      if (this->sync_ && dialog && !dialog->getContext())
      {
        // Boundary apply precedes the normal native layout pass. Project this
        // probe's Scene now so the existing friend can remove the presenter
        // before any run-loop entry, while still inside the apply Operation.
        IPlatformController *controller = loka::dsl::testing::SceneTestAccess::platformController(*this->scene());
        if (controller)
          controller->onChange(loka::dsl::testing::SceneTestAccess::rootNode(*this->scene()), NODE_DIRTY_CHILD, false);
      }
      if (this->sync_ && dialog && dialog->getContext())
      {
        this->sync_ = false;
        MacDialogResultTestAccess::synchronous(*static_cast<MacOpenFileDialogContext *>(dialog->getContext()));
      }
    }
    void show(bool save, bool synchronous)
    {
      this->sync_ = synchronous;
      this->shown[save ? 1 : 0].set(true);
    }
    void hide(bool save) { this->shown[save ? 1 : 0].set(false); }
  private:
    bool sync_;
    NodeState<bool> shown[2];
    NodeState<FileChooserResult> result;
  };
}

class DialogProbe;
@interface RetirementProbeTimer : NSObject
{
  DialogProbe *owner_;
}
- (id)initWithOwner:(DialogProbe *)owner;
- (void)tick:(NSTimer *)timer;
- (void)detach;
@end

/** Config owns its image and modal sampler. The timer borrows only this config,
    and is invalidated before config destruction. Nodes remain App-owned. */
class DialogProbe : public AppConfigurable
{
public:
  explicit DialogProbe(PlatformContext *context)
      : AppConfigurable(context), log_(L"retirement-probe-dialog.log"), app_(0), root_(0), viewer_(0),
        open_(), image_(), phase_(0), stage_(MOUNT), ticks_(0), since_(0), timer_(nil), target_(nil)
  {
    this->log_.note("fixture=modal img=1024x768 pixel_bytes=3145728 dwell_seconds=2");
    this->log_.note("note=replacement-uses-existing-dialog-result-return-port; out-of-process-panel-has-no-public-selection-setter");
  }
  ~DialogProbe() { this->disarm(); }
  bool ready() const { return this->log_.valid(); }
  int exitCode() const { return this->stage_ == DONE && this->log_.valid() ? 0 : 1; }
  void setApp(App *app) { this->app_ = app; }
  virtual void compose(AppComposition &composition)
  {
    composition << loka::scenario_tests::MakeScenarioWindow<DialogProps, DialogRoot>(
        DialogProps(), &this->root_, 340, 274, "Panel retirement probe", IdlePolicy::interval(0.05), &OnIdle, this);
    composition << loka::scenario_tests::MakeScenarioWindow<simpleviewer::MainProps, simpleviewer::MainNode>(
        simpleviewer::MainProps().platformContext(this->getPlatformContext()).openDialogEvent(&this->open_),
        &this->viewer_, 480, 280, "Viewer replacement probe", IdlePolicy::interval(0.05), 0, 0);
  }
  void timerTick()
  {
    if (!this->log_.valid()) { this->fail("log-failed-during-modal"); return; }
    const double now = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *panel = [NSApp modalWindow];
    if (this->stage_ == SEEK)
    {
      if (!panel) { if (now - this->since_ > 10) this->fail("panel-open-timeout"); return; }
      if (loka::core::Operation::hasActive() != (this->phase_ >= 2))
      { this->fail("unexpected-modal-operation-path"); return; }
      this->sample("open"); this->stage_ = DWELL; this->since_ = now;
    }
    else if (this->stage_ == DWELL && now - this->since_ >= 1)
    {
      this->sample("dwell-before-drop"); this->image_ = Image(); this->sample("dwell-after-drop");
      this->stage_ = DROPPED; this->since_ = now;
    }
    else if (this->stage_ == DROPPED && now - this->since_ >= 1)
    {
      this->sample("dwell-late"); this->stage_ = DISMISS; this->since_ = now;
      if ([panel respondsToSelector:@selector(cancel:)])
      { this->log_.note("dismiss-attempt=cancel:"); [(id)panel cancel:nil]; }
      else { this->stage_ = ABORT_DISMISS; this->log_.note("dismiss-attempt=abortModal"); [NSApp abortModal]; }
    }
    else if (this->stage_ == DISMISS && panel && now - this->since_ >= 1)
    {
      this->stage_ = ABORT_DISMISS; this->since_ = now;
      this->log_.note("dismiss-attempt=abortModal reason=cancel-timeout");
      [NSApp abortModal];
    }
    else if (this->stage_ == ABORT_DISMISS && panel && now - this->since_ >= 2)
      this->fail("panel-dismiss-timeout");
  }
private:
  enum Stage { MOUNT, START, SEEK, DWELL, DROPPED, DISMISS, ABORT_DISMISS, SETTLE, PICK_A, LOAD_A, PICK_B, REPLACE, REPLACE_SETTLE, DONE };
  static void OnIdle(Window *window, double, void *data) { static_cast<DialogProbe *>(data)->idle(window); }
  void disarm()
  {
    [this->timer_ invalidate]; [this->timer_ release]; this->timer_ = nil;
    [this->target_ detach]; [this->target_ release]; this->target_ = nil;
  }
  void fail(const char *reason)
  {
    std::fprintf(stderr, "error=%s\n", reason);
    this->log_.error(reason);
    [this->timer_ invalidate];
    if ([NSApp modalWindow]) [NSApp abortModal];
    this->app_->quit();
  }
  void sample(const char *point) { this->log_.sample(*this->getPlatformContext(), this->ticks_, point, 0, 1024, 768); }
  void replacementSample(const char *point)
  {
    int width = 0, height = 0;
    simpleviewer::testing::MainAccess::dimensions(*this->viewer_, width, height);
    this->log_.sample(*this->getPlatformContext(), this->ticks_, point, 0, width, height);
  }
  bool decodeFixture()
  {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    const loka::core::resource::Blob blob = retirement_probe::Bitmap(1024, 768, 0x80);
    const bool decoded = this->getPlatformContext()->createImageFromBlob(blob, 0, blob.size(), this->image_)
        && this->image_.width() == 1024 && this->image_.height() == 768;
    [pool drain]; // NSData decode scratch must not survive a synchronous dwell.
    return decoded;
  }
  bool pick(bool first)
  {
    const int width = first ? 1024 : 800, height = first ? 768 : 600;
    const loka::core::resource::Blob blob = retirement_probe::Bitmap(width, height, first ? 0x40 : 0xc0);
    const char *directory = std::getenv("LOKA_RETIREMENT_PROBE_OUTPUT_DIR");
    NSString *dir = directory ? [NSString stringWithUTF8String:directory]
        : [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
    NSString *path = [dir stringByAppendingPathComponent:first ? @"probe-a.bmp" : @"probe-b.bmp"];
    loka::platform::file::FileHandle handle;
    handle.displayPath = loka::core::String::Utf8([path fileSystemRepresentation], std::strlen([path fileSystemRepresentation]));
    handle.kind = loka::file::File::KIND_FILE;
    FILE *stream = loka::platform::file::OpenWriteTruncate(handle);
    if (!stream) return false;
    const bool written = std::fwrite(blob.data(), 1, blob.size(), stream) == blob.size();
    const bool flushed = loka::platform::file::FlushWrite(stream, handle);
    const bool closed = std::fclose(stream) == 0;
    if (!written || !flushed || !closed) return false;
    this->open_.emit();
    loka::core::Operation *turn = loka::core::testing::OperationTestAccess::active();
    if (!turn) return false;
    turn->settle();
    // Materialize the real SimpleViewer dialog; capture before the scheduled
    // presenter runs. ReturnPort delivery still happens at a later rail entry.
    this->viewer_->scene()->getWindow()->flushSceneInvalidation();
    OpenFileDialogNode *dialog = FindDialog(this->viewer_);
    if (!dialog || !dialog->getContext()) return false;
    return MacDialogResultTestAccess::returnFile(*static_cast<MacOpenFileDialogContext *>(dialog->getContext()),
        loka::file::File::FromPath(handle.displayPath));
  }
  void idle(Window *window)
  {
    if (!this->log_.valid()) { this->app_->quit(); return; }
    switch (this->stage_)
    {
    case MOUNT:
      if (!this->root_ || !this->viewer_) { if (++this->ticks_ == 100) this->fail("mount-timeout"); return; }
      this->stage_ = START;
      return;
    case START:
    {
      const char *names[] = {"open-deferred", "save-deferred", "open-sync", "save-sync"};
      this->ticks_ = 0; this->log_.begin(names[this->phase_]);
      if (!this->decodeFixture()) { this->fail("decode-failed"); return; }
      this->sample("pre");
      this->target_ = [[RetirementProbeTimer alloc] initWithOwner:this];
      this->timer_ = [[NSTimer timerWithTimeInterval:0.1 target:this->target_ selector:@selector(tick:) userInfo:nil repeats:YES] retain];
      if (!this->target_ || !this->timer_) { this->fail("timer-failed"); return; }
      [[NSRunLoop mainRunLoop] addTimer:this->timer_ forMode:NSModalPanelRunLoopMode];
      [[NSRunLoop mainRunLoop] addTimer:this->timer_ forMode:NSDefaultRunLoopMode];
      this->stage_ = SEEK; this->since_ = [NSDate timeIntervalSinceReferenceDate];
      this->root_->show(this->phase_ % 2 != 0, this->phase_ >= 2);
      window->flushSceneInvalidation();
      return;
    }
    case SEEK: case DWELL: case DROPPED: return;
    case DISMISS:
    case ABORT_DISMISS:
      if ([NSApp modalWindow]) return;
      this->log_.note(this->stage_ == DISMISS ? "dismiss=cancel:" : "dismiss=abortModal");
      this->disarm(); this->root_->hide(this->phase_ % 2 != 0); window->flushSceneInvalidation();
      this->stage_ = SETTLE; this->ticks_ = 0;
      return;
    case SETTLE:
      this->sample("after-dismiss");
      if (++this->ticks_ == 5)
      {
        this->log_.summary();
        if (++this->phase_ == 4) this->stage_ = PICK_A;
        else this->stage_ = START;
      }
      return;
    case PICK_A:
      this->log_.begin("replace"); this->ticks_ = 0; this->replacementSample("pre");
      if (!this->pick(true)) { this->fail("pick-a-failed"); return; }
      this->stage_ = LOAD_A; return;
    case LOAD_A:
    {
      this->replacementSample("load-a");
      int width, height; simpleviewer::testing::MainAccess::dimensions(*this->viewer_, width, height);
      if (width == 1024 && height == 768) { this->stage_ = PICK_B; this->ticks_ = 0; }
      else if (++this->ticks_ == 100) this->fail("load-a-timeout");
      return;
    }
    case PICK_B:
      if (!this->pick(false)) { this->fail("pick-b-failed"); return; }
      this->stage_ = REPLACE; this->ticks_ = 0; return;
    case REPLACE:
      this->replacementSample("replace");
      if (++this->ticks_ == 10)
      {
        int width, height; simpleviewer::testing::MainAccess::dimensions(*this->viewer_, width, height);
        if (width != 800 || height != 600) { this->fail("load-b-timeout"); return; }
        this->stage_ = REPLACE_SETTLE; this->ticks_ = 0;
      }
      return;
    case REPLACE_SETTLE:
      this->replacementSample("settle");
      if (++this->ticks_ == 5) { this->log_.summary(); this->stage_ = DONE; this->app_->quit(); }
      return;
    case DONE: return;
    }
  }
  RetirementProbeLog log_;
  App *app_;
  DialogRoot *root_;
  simpleviewer::MainNode *viewer_;
  loka::core::EmitterState open_;
  Image image_;
  int phase_;
  Stage stage_;
  int ticks_;
  double since_;
  NSTimer *timer_;
  RetirementProbeTimer *target_;
};
@implementation RetirementProbeTimer
- (id)initWithOwner:(DialogProbe *)owner { self = [super init]; if (self) owner_ = owner; return self; }
- (void)tick:(NSTimer *)timer { (void)timer; if (owner_) owner_->timerTick(); }
- (void)detach { owner_ = 0; }
@end
int main()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]; (void)pool;
  loka::platform::InitPlatformRuntime();
  loka::core::ScopedPtr<PlatformContext> context(loka::platform::CreatePlatformContext());
  if (!context.get()) { std::fprintf(stderr, "error=platform-context-create-failed\n"); return 1; }
  DialogProbe config(context.get());
  if (!config.ready()) return 1;
  loka::core::ScopedPtr<App> app(context->createApp(&config, 0, 0));
  if (!app.get()) { std::fprintf(stderr, "error=app-create-failed\n"); return 1; }
  config.setApp(app.get()); app->run(); return config.exitCode();
}
