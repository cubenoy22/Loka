#include "MacOpenFileDialogTests.hpp"
#include "support/TestVerify.hpp"
#include "support/DialogResultTestAccess.hpp"
#include <AppKit/AppKit.h>
#include <cstring>
#include <new>
#include "core/LokaAlloc.hpp"
#include "MacWindow.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "app/core/App.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "testing/app/AppTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"

/** Pause the actual scheduled operation at its native-equivalent return port. */
class MacDialogResultTestAccess
{
public:
  static loka::app::DialogResultTransport::Registration *capture(MacOpenFileDialogContext &context)
  {
    LOKA_VERIFY(context.registration_ != 0);
    [(id)context.deferredPresenter_ performSelector:@selector(cancelPresent)];
    context.presentation_.markPresented();
    return context.registration_;
  }
  static NSTimer *scheduledTimer(MacOpenFileDialogContext &context)
  {
    return (NSTimer *)context.scheduledTimerForTesting();
  }
};

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  typedef loka::app::testing::AppTestAccess AppAccess;
  typedef loka::app::testing::DialogResultTestAccess TransportAccess;

  int g_dialogEntries = 0;
  void *countedAllocate(size_t size, const loka::core::LokaAllocationSite &site)
  {
    void *storage = new (std::nothrow) unsigned char[size];
    if (storage && std::strcmp(site.ownerTag, "DialogResultTransport") == 0)
      ++g_dialogEntries;
    return storage;
  }
  void countedFree(void *storage, const loka::core::LokaAllocationSite &site)
  {
    if (std::strcmp(site.ownerTag, "DialogResultTransport") == 0)
      --g_dialogEntries;
    delete[] static_cast<unsigned char *>(storage);
  }

  class DialogApp : public App
  {
  public:
    DialogApp() : App(0), closes(0) {}
    virtual void quit() {}
    void install(MacWindow *window)
    {
      this->group_ = new AppComponentGroup(std::vector<AppComponent *>(1, window));
      window->setApp(this);
    }
    int closes;
  protected:
    virtual void windowClosed(Window *window)
    {
      ++this->closes;
      App::windowClosed(window);
    }
  };

  class DialogRoot;
  /** Stack observations survive root/Window reclamation; no callback owns them. */
  struct Probe
  {
    Probe() : root(0), app(0), duringApply(0), writes(0), emits(0), unmounts(0), destroy(false) {}
    DialogRoot *root;
    DialogApp *app;
    DialogResultTransport::ReturnPort *duringApply;
    int writes;
    int emits;
    int unmounts;
    bool destroy;
    static void count(void *data) { ++*static_cast<int *>(data); }
  };
  Probe *g_probe = 0;
  typedef BoundaryPropsFor<DialogRoot> DialogProps;
  class DialogRoot : public StdCompositionBoundaryNodeBase<DialogProps>
  {
    typedef StdCompositionBoundaryNodeBase<DialogProps> Base;
  public:
    explicit DialogRoot(const DialogProps &props) : Base(props), probe_(*g_probe)
    {
      this->probe_.root = this;
      this->state(this->shown, true);
      this->state(this->result, FileChooserResult());
    }
    virtual ~DialogRoot()
    {
      this->result.unbind(&Probe::count, &this->probe_.writes);
      this->emitter.unbind(&Probe::count, &this->probe_.emits);
      this->probe_.root = 0;
    }
    virtual void composeNode(NodeComposition &composition)
    {
      ShowDefinition seat = Show(*this->shown.state());
      if (this->probe_.destroy)
        seat.destroyOnDetach();
      composition.declare(seat << OpenFileDialog().result(this->result).onResult(&this->emitter));
    }
    virtual void detachNode(NodeComposition &) { ++this->probe_.unmounts; }
    virtual void applyPendingUpdate(const PlatformApplyPlan &plan)
    {
      Base::applyPendingUpdate(plan);
      DialogResultTransport::ReturnPort *port = this->probe_.duringApply;
      if (!port)
        return;
      this->probe_.duringApply = 0;
      LOKA_VERIFY(this->scene()->isRunInProgress());
      LOKA_VERIFY(port->seal(FileChooserResult::Error(33)) == this->scene()->getWindow());
      // Nested admission cannot deliver into or reclaim the current Scene run.
      AppAccess::flushWindowInvalidations(*this->probe_.app);
      LOKA_VERIFY(this->probe_.writes == 0 && this->probe_.emits == 0);
    }
    NodeState<bool> shown;
    NodeState<FileChooserResult> result;
    loka::core::EmitterState emitter;
  private:
    Probe &probe_;
  };

  OpenFileDialogNode *findDialog(Node *node)
  {
    if (!node)
      return 0;
    if (node->asOpenFileDialogNode())
      return node->asOpenFileDialogNode();
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
    {
      OpenFileDialogNode *dialog = findDialog(child);
      if (dialog)
        return dialog;
    }
    return 0;
  }

  struct Fixture
  {
    explicit Fixture(bool destroy = false) : window(0)
    {
      [NSApplication sharedApplication];
      this->probe.destroy = destroy;
      this->probe.app = &this->app;
      g_probe = &this->probe;
      WindowProps props;
      props.scene(new Scene(BoundaryDefinition<DialogProps, DialogRoot>(DialogProps())));
      this->window = new MacWindow(&this->platform, props);
      this->app.install(this->window);
      LOKA_VERIFY(this->probe.root != 0);
      LOKA_VERIFY(loka::dsl::testing::SceneTestAccess::rootNode(*this->window->scene()) == this->probe.root);
      // Observe after mount: constructor declarations have no State yet.
      LOKA_VERIFY(this->probe.root->result.isValid());
      this->probe.root->result.bind(&Probe::count, &this->probe.writes, false);
      this->probe.root->emitter.bind(&Probe::count, &this->probe.emits, false);
      LOKA_VERIFY(this->dialog() != 0 && this->dialog()->getContext() != 0);
      LOKA_VERIFY(TransportAccess::census(this->window->dialogResults()) == 1);
    }
    ~Fixture() { g_probe = 0; }
    OpenFileDialogNode *dialog() { return findDialog(this->probe.root); }
    MacOpenFileDialogContext &context()
    {
      return *static_cast<MacOpenFileDialogContext *>(this->dialog()->getContext());
    }
    void flush() { AppAccess::flushWindowInvalidations(this->app); }
    void hideDialog()
    {
      this->probe.root->shown.set(false);
      // Retire before admission can consume the pending result.
      this->window->flushSceneInvalidation();
    }
    void silent() const { LOKA_VERIFY(this->probe.writes == 0 && this->probe.emits == 0); }
    NullPlatformContext platform;
    Probe probe;
    DialogApp app;
    MacWindow *window; // Borrowed from app; invalid after terminal close drain.
  };
}

void testMacOpenFileDialogRetiredBeforeCompletion()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  {
    Fixture fixture(true);
    // Existing cancel-before-timer coverage: no defect/red claim on the base.
    NSTimer *timer = [MacDialogResultTestAccess::scheduledTimer(fixture.context()) retain];
    LOKA_VERIFY(timer != nil && [timer isValid]);
    fixture.hideDialog();
    LOKA_VERIFY(fixture.dialog() == 0);
    LOKA_VERIFY(![timer isValid]);
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]];
    fixture.flush();
    fixture.silent();
    LOKA_VERIFY(TransportAccess::census(fixture.window->dialogResults()) == 0);
    [timer release];

    fixture.probe.root->shown.set(true);
    fixture.flush();
    DialogResultTransport::ReturnPort port(MacDialogResultTestAccess::capture(fixture.context()));
    fixture.hideDialog();
    fixture.flush(); // Reclaim the reservation before simulated native return.
    LOKA_VERIFY(TransportAccess::census(fixture.window->dialogResults()) == 0);
    LOKA_VERIFY(port.seal(FileChooserResult::Error(33)) == 0);
    fixture.flush();
    fixture.silent();
  }
  [pool drain];
}

void testMacOpenFileDialogRetainedDetachDropsResult()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  {
    Fixture fixture;
    OpenFileDialogNode *dialog = fixture.dialog();
    MacOpenFileDialogContext *context = &fixture.context();
    DialogResultTransport::ReturnPort port(MacDialogResultTestAccess::capture(*context));
    LOKA_VERIFY(port.seal(FileChooserResult::Error(33)) == fixture.window);
    fixture.hideDialog();
    LOKA_VERIFY(dialog->getContext() == context);
    const NodeLifecycleFact fact = dialog->lifecycleFact();
    LOKA_VERIFY(fact == NODE_FACT_DETACHED_RETAINED);
    fixture.flush();
    fixture.silent();
    LOKA_VERIFY(fixture.probe.root->result.get().kind == FileChooserResult::RESULT_NONE);
    LOKA_VERIFY(TransportAccess::census(fixture.window->dialogResults()) == 0);
    fixture.probe.root->shown.set(true);
    fixture.flush();
    DialogResultTransport::ReturnPort fresh(MacDialogResultTestAccess::capture(fixture.context()));
    fresh.seal(FileChooserResult::Canceled());
    fixture.silent();
    fixture.flush();
    LOKA_VERIFY(fixture.probe.writes == 1 && fixture.probe.emits == 1);
    fixture.flush();
    LOKA_VERIFY(fixture.probe.writes == 1 && fixture.probe.emits == 1);
    LOKA_VERIFY(TransportAccess::census(fixture.window->dialogResults()) == 0);
  }
  [pool drain];
}

void testMacOpenFileDialogCompletionDuringSceneWaitsForAdmission()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  {
    Fixture fixture;
    DialogResultTransport::ReturnPort port(MacDialogResultTestAccess::capture(fixture.context()));
    fixture.probe.duringApply = &port;
    fixture.window->scene()->requestInvalidate(NODE_DIRTY_LAYOUT);
    fixture.flush();
    LOKA_VERIFY(fixture.probe.duringApply == 0);
    fixture.silent();
    fixture.flush();
    LOKA_VERIFY(fixture.probe.writes == 1 && fixture.probe.emits == 1);
    LOKA_VERIFY(fixture.probe.root->result.get().kind == FileChooserResult::RESULT_ERROR);
    fixture.flush();
    LOKA_VERIFY(fixture.probe.writes == 1 && fixture.probe.emits == 1);
    LOKA_VERIFY(TransportAccess::census(fixture.window->dialogResults()) == 0);
  }
  [pool drain];
}

void testMacOpenFileDialogCloseDropsPendingAtDrain()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  // Keep the injected backend installed through each entire owner lifetime.
  loka::core::LokaAllocSetBackend(&countedAllocate, &countedFree);
  for (int pending = 0; pending != 2; ++pending)
  {
    Fixture fixture;
    if (pending)
    {
      DialogResultTransport::ReturnPort port(MacDialogResultTestAccess::capture(fixture.context()));
      port.seal(FileChooserResult::Canceled());
    }
    LOKA_VERIFY(g_dialogEntries == 1);
    fixture.window->handleWindowWillClose();
    // A scheduled presenter may run before the close drain retires its context.
    fixture.context().presentDeferred();
    fixture.silent();
    LOKA_VERIFY(fixture.app.closes == 0 && fixture.probe.unmounts == 0);
    LOKA_VERIFY(AppAccess::pendingWindowCloseCount(fixture.app) == 1);
    LOKA_VERIFY(TransportAccess::census(fixture.window->dialogResults()) == 1);
    fixture.flush();
    fixture.silent();
    LOKA_VERIFY(fixture.app.closes == 1 && fixture.probe.unmounts == 1);
    LOKA_VERIFY(fixture.probe.root == 0);
    LOKA_VERIFY(g_dialogEntries == 0);
    LOKA_VERIFY(AppAccess::pendingWindowCloseCount(fixture.app) == 0);
    fixture.flush();
    fixture.silent();
    LOKA_VERIFY(fixture.app.closes == 1);
  }
  LOKA_VERIFY(g_dialogEntries == 0);
  loka::core::LokaAllocSetBackend(0, 0);
  [pool drain];
}

void testMacOpenFileDialogRetargetDropsResult()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  {
    int replacementEmits = 0;
    loka::core::EmitterState replacement;
    replacement.bind(&Probe::count, &replacementEmits, false);
    Fixture fixture;
    DialogResultTransport::ReturnPort port(MacDialogResultTestAccess::capture(fixture.context()));
    port.seal(FileChooserResult::Canceled());
    fixture.context().onPropsApplied(); // Equal identities preserve the registration.
    LOKA_VERIFY(TransportAccess::census(fixture.window->dialogResults()) == 1);
    fixture.dialog()->props = OpenFileDialogProps().onResult(&replacement);
    fixture.context().onPropsApplied();
    fixture.flush();
    fixture.silent();
    LOKA_VERIFY(replacementEmits == 0);
    replacement.unbind(&Probe::count, &replacementEmits);
    LOKA_VERIFY(TransportAccess::census(fixture.window->dialogResults()) == 0);
  }
  [pool drain];
}
