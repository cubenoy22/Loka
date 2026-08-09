#include "MacOpenFileDialogContext.hpp"
#include <cassert>
#include "../MacScenePlatformController.hpp"
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
@end

@implementation LokaMacOpenFileDialogDeferredPresenter
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

namespace
{
  struct MacOpenNativeDialogSession
  {
    MacOpenNativeDialogSession()
        : disposed(false)
    {
    }

    bool disposed;
  };

  static void DeliverOpenFileDialogResult(loka::core::MutableState<loka::app::FileChooserResult> *resultState,
                                          loka::core::EmitterState *onResult,
                                          const loka::app::FileChooserResult &result)
  {
    void *onResultToken = onResult ? onResult->retainExternalLifetimeToken() : 0;
    if (resultState)
    {
      resultState->set(result, true);
    }
    if (onResult && loka::core::StateBase::isExternalLifetimeTokenAlive(onResultToken))
    {
      onResult->emit();
    }
    if (onResultToken)
    {
      loka::core::StateBase::releaseExternalLifetimeToken(onResultToken);
    }
  }

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

    static void afterAttach(MacOpenFileDialogContext *ctx)
    {
      // Keep presentation in the shared after-attach slot; see RetainedNodeHandler.
      ctx->presentIfNeeded();
    }
  };

  MacOpenFileDialogNodeHandler gMacOpenFileDialogNodeHandler;
} // namespace

struct MacOpenFileDialogContext::NativeDialogSession : public MacOpenNativeDialogSession
{
};

MacOpenFileDialogContext::MacOpenFileDialogContext(MacScenePlatformController *controller,
                                                   void *parentView,
                                                   loka::app::OpenFileDialogNode *node)
    : MacRetirableContext(controller),
      node_(node),
      resultState_(0),
      onResult_(0),
      presentation_(),
      deferredPresenter_(0),
      dialog_(0)
{
  (void)parentView;
  resultState_ = node_ ? node_->props.result_ : 0;
  onResult_ = node_ ? node_->props.onResult_ : 0;
  deferredPresenter_ = [[LokaMacOpenFileDialogDeferredPresenter alloc] initWithOwner:this];
}

MacOpenFileDialogContext::~MacOpenFileDialogContext()
{
  assert(!deferredPresenter_ && !dialog_ && "terminal fact delivery must queue the presenter before context reclaim");
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
      this->resultState_ = 0;
      this->onResult_ = 0;
    }
  }
}

void MacOpenFileDialogContext::applyAttachedPresentation()
{
  presentIfNeeded();
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

void MacOpenFileDialogContext::presentIfNeeded()
{
  if (dialog_ || !presentation_.beginPresent())
  {
    return;
  }
  dialog_ = new NativeDialogSession();
  if (deferredPresenter_)
  {
    [(LokaMacOpenFileDialogDeferredPresenter *)deferredPresenter_ schedulePresent];
    return;
  }
  presentDialog();
}

void MacOpenFileDialogContext::presentDeferred()
{
  presentDialog();
}

void MacOpenFileDialogContext::presentDialog()
{
  if (!presentation_.isPresenting())
  {
    return;
  }
  NativeDialogSession *dialog = dialog_;
  if (!dialog || dialog->disposed)
  {
    return;
  }
  loka::core::MutableState<loka::app::FileChooserResult> *resultState = resultState_;
  loka::core::EmitterState *onResult = onResult_;
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  if (!panel)
  {
    dialog = this->detachDialogIfActive(dialog);
    if (!dialog)
    {
      return;
    }
    presentation_.markPresented();
    DeliverOpenFileDialogResult(resultState, onResult, loka::app::FileChooserResult::Error(1));
    delete dialog;
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
  dialog = this->detachDialogIfActive(dialog);
  if (!dialog)
  {
    return;
  }
  presentation_.markPresented();
  DeliverOpenFileDialogResult(resultState, onResult, result);
  delete dialog;
}

void MacOpenFileDialogContext::setResult(const loka::app::FileChooserResult &result)
{
  DeliverOpenFileDialogResult(resultState_, onResult_, result);
}

void MacOpenFileDialogContext::disposeDialog()
{
  if (!dialog_)
  {
    return;
  }
  dialog_->disposed = true;
  delete dialog_;
  dialog_ = 0;
}

MacOpenFileDialogContext::NativeDialogSession *
MacOpenFileDialogContext::detachDialogIfActive(NativeDialogSession *dialog)
{
  if (!dialog || dialog_ != dialog || dialog->disposed)
  {
    return 0;
  }
  dialog_ = 0;
  dialog->disposed = true;
  return dialog;
}

void RegisterMacOpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gMacOpenFileDialogNodeHandler);
}
