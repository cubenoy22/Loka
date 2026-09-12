#include "MacOpenFileDialogContext.hpp"
#include <cassert>
#include "../MacScenePlatformController.hpp"
#include "../MacWindow.hpp"
#include "MacObjCCompat.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "Utf8String.hpp"
#import <AppKit/AppKit.h>

@interface LokaMacOpenFileDialogDeferredPresenter : NSObject
{
@private
  MacOpenFileDialogContext *owner_;
  NSTimer *timer_;
}
- (id)initWithOwner:(MacOpenFileDialogContext *)owner;
- (void)schedulePresent;
- (void)cancelPresent;
- (void)detachOwner;
- (void)onTimer:(NSTimer *)timer;
#ifdef TEST_BUILD
@property(nonatomic, assign, readonly) NSTimer *scheduledTimerForTesting;
#endif
@end

@implementation LokaMacOpenFileDialogDeferredPresenter
#ifdef TEST_BUILD
- (NSTimer *)scheduledTimerForTesting
{
  return timer_;
}
#endif
- (id)initWithOwner:(MacOpenFileDialogContext *)owner
{
  self = [super init];
  if (self)
  {
    owner_ = owner;
    timer_ = nil;
  }
  return self;
}

- (void)dealloc
{
  [self cancelPresent];
  [super dealloc];
}

- (void)schedulePresent
{
  if (timer_)
  {
    return;
  }
  timer_ = [[NSTimer scheduledTimerWithTimeInterval:0.0
                                             target:self
                                           selector:@selector(onTimer:)
                                           userInfo:nil
                                            repeats:NO] retain];
}

- (void)cancelPresent
{
  if (!timer_)
  {
    return;
  }
  [timer_ invalidate];
  [timer_ release];
  timer_ = nil;
}

- (void)detachOwner
{
  owner_ = 0;
}

- (void)onTimer:(NSTimer *)timer
{
  if (timer != timer_)
  {
    return;
  }
  [timer_ release];
  timer_ = nil;
  if (owner_)
  {
    owner_->presentDeferred();
  }
}
@end

#ifdef TEST_BUILD
void *MacOpenFileDialogContext::scheduledTimerForTesting() const
{
  return [(LokaMacOpenFileDialogDeferredPresenter *)this->deferredPresenter_ scheduledTimerForTesting];
}
#endif

namespace
{
  class MacOpenFileDialogNodeHandler
      : public loka::app::scene::RetainedNodeHandler<MacOpenFileDialogNodeHandler,
                                                     loka::app::OpenFileDialogNode,
                                                     MacOpenFileDialogContext>
  {
  public:
    static loka::app::OpenFileDialogNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asOpenFileDialogNode() : 0;
    }

    static MacOpenFileDialogContext *create(loka::app::OpenFileDialogNode *dialog,
                                            loka::app::scene::IPlatformController *controller,
                                            const loka::app::scene::LayoutState &state)
    {
      (void)state;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      return new MacOpenFileDialogContext(mac, mac->rootView(), dialog);
    }

    static void refresh(MacOpenFileDialogContext *ctx, const loka::app::scene::LayoutState &)
    {
      ctx->presentIfNeeded();
    }

    static void afterAttach(MacOpenFileDialogContext *ctx)
    {
      // Keep presentation in the shared after-attach slot; see RetainedNodeHandler.
      ctx->presentIfNeeded();
    }
  };

  MacOpenFileDialogNodeHandler gMacOpenFileDialogNodeHandler;
} // namespace

MacOpenFileDialogContext::MacOpenFileDialogContext(MacScenePlatformController *controller,
                                                   void *parentView,
                                                   loka::app::OpenFileDialogNode *node)
    : MacRetirableContext(controller),
      node_(node),
      transport_(0),
      presentation_(),
      deferredPresenter_(0),
      registration_(0)
{
  MacWindow *window = MacWindow::fromRootView(parentView);
  this->transport_ = window ? &window->dialogResults() : 0;
  deferredPresenter_ = [[LokaMacOpenFileDialogDeferredPresenter alloc] initWithOwner:this];
}

MacOpenFileDialogContext::~MacOpenFileDialogContext()
{
  assert(!deferredPresenter_ && !registration_ && "terminal fact delivery must queue the presenter before context reclaim");
}

void MacOpenFileDialogContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void MacOpenFileDialogContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                             loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (next == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
  else
  {
    // DETACHED_RETAINED hides; terminal RETIRED keeps the same policy
    // (hide before the ritual destroys the native pair).
    this->applyDetachedPresentation();
    if (next == loka::app::scene::NODE_FACT_RETIRED)
    {
      [(LokaMacOpenFileDialogDeferredPresenter *)this->deferredPresenter_ detachOwner];
      this->retireNativeObject(this->deferredPresenter_);
      this->node_ = 0;
    }
  }
}

void MacOpenFileDialogContext::applyAttachedPresentation()
{
  this->presentation_.markDetached();
}

void MacOpenFileDialogContext::applyDetachedPresentation()
{
  presentation_.markDetached();
  if (deferredPresenter_)
  {
    [(LokaMacOpenFileDialogDeferredPresenter *)deferredPresenter_ cancelPresent];
  }
  this->disposeDialog();
}

void MacOpenFileDialogContext::onPropsApplied()
{
  if (this->registration_ && this->node_ && !this->registration_->matches(this->node_->props))
  {
    this->applyDetachedPresentation();
    // Like Win32, retarget abandons this operation; reattach permits a new one.
    this->presentation_.markPresented();
  }
}

void MacOpenFileDialogContext::presentIfNeeded()
{
  if (this->registration_ || !this->transport_ || !this->node_
      || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return;
  if (!this->presentation_.beginPresent())
    return;
  this->registration_ = this->transport_->reserve(this->node_->props);
  if (!this->registration_)
  {
    this->presentation_.markDetached();
    return;
  }
  if (this->deferredPresenter_)
  {
    [(LokaMacOpenFileDialogDeferredPresenter *)this->deferredPresenter_ schedulePresent];
    return;
  }
  this->presentDialog();
}

void MacOpenFileDialogContext::presentDeferred()
{
  presentDialog();
}

void MacOpenFileDialogContext::presentDialog()
{
  // Native close revokes the captured props before Scene teardown cancels
  // the timer. Refuse that queued presenter as well as a retargeted operation.
  if (!this->presentation_.isPresenting() || !this->registration_ || !this->node_
      || !this->registration_->matches(this->node_->props))
  {
    return;
  }
  loka::app::DialogResultTransport::ReturnPort port(this->registration_);
  this->presentation_.markPresented();
  // From here through modal return, only the revocable stack port is borrowed.
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  if (!panel)
  {
    port.seal(loka::app::FileChooserResult::Error(1));
    return;
  }

  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseDirectories:NO];
  [panel setCanChooseFiles:YES];
  NSInteger response = [panel runModal];
  loka::app::FileChooserResult result = loka::app::FileChooserResult::Canceled();
  if (response == LOKA_MAC_MODAL_RESPONSE_OK)
  {
    NSURL *url = [panel URL];
    if (url)
    {
      std::string path = loka::macos::Utf8FromNSString([url path]);
      loka::file::File file = loka::file::File::FromPath(loka::core::String(path));
      file.setKind(loka::file::File::KIND_FILE);
      result = loka::app::FileChooserResult::File(file);
    }
    else
    {
      result = loka::app::FileChooserResult::Error(2);
    }
  }
  // MacApp's repeating admission timer services this pending fact. No callback,
  // context access or separate wake allocation is needed on the native return.
  port.seal(result);
}

void MacOpenFileDialogContext::disposeDialog()
{
  loka::app::DialogResultTransport::Registration *registration = this->registration_;
  this->registration_ = 0;
  delete registration;
}

void RegisterMacOpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gMacOpenFileDialogNodeHandler);
}
