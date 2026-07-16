#include "MacOpenFileDialogContext.hpp"
#include "../MacScenePlatformController.hpp"
#include "MacObjCCompat.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
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
                                          loka::core::MutableState<bool> *closeState,
                                          const loka::app::FileChooserResult &result)
  {
    void *onResultToken = onResult ? onResult->retainExternalLifetimeToken() : 0;
    void *closeStateToken = closeState ? closeState->retainExternalLifetimeToken() : 0;
    if (resultState)
    {
      resultState->set(result, true);
    }
    if (onResult && loka::core::StateBase::isExternalLifetimeTokenAlive(onResultToken))
    {
      onResult->emit();
    }
    if (closeState && loka::core::StateBase::isExternalLifetimeTokenAlive(closeStateToken) && closeState->get())
    {
      closeState->set(false, true);
    }
    if (onResultToken)
    {
      loka::core::StateBase::releaseExternalLifetimeToken(onResultToken);
    }
    if (closeStateToken)
    {
      loka::core::StateBase::releaseExternalLifetimeToken(closeStateToken);
    }
  }

  class MacOpenFileDialogNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::OpenFileDialogNode *dialog = node ? node->asOpenFileDialogNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!dialog || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureOpenFileDialogContext(dialog);
    }
  };

  MacOpenFileDialogNodeHandler gMacOpenFileDialogNodeHandler;
} // namespace

struct MacOpenFileDialogContext::NativeDialogSession : public MacOpenNativeDialogSession
{
};

MacOpenFileDialogContext::MacOpenFileDialogContext(void *parentView, loka::app::OpenFileDialogNode *node)
    : parentView_(parentView),
      node_(node),
      resultState_(0),
      onResult_(0),
      closeState_(0),
      presentation_(),
      deferredPresenter_(0),
      dialog_(0)
{
  resultState_ = node_ ? node_->props.result_ : 0;
  onResult_ = node_ ? node_->props.onResult_ : 0;
  closeState_ = node_ ? node_->props.closeState_ : 0;
  deferredPresenter_ = [[LokaMacOpenFileDialogDeferredPresenter alloc] initWithOwner:this];
}

MacOpenFileDialogContext::~MacOpenFileDialogContext()
{
  if (deferredPresenter_)
  {
    [(LokaMacOpenFileDialogDeferredPresenter *)deferredPresenter_ cancelPresent];
    [(LokaMacOpenFileDialogDeferredPresenter *)deferredPresenter_ release];
    deferredPresenter_ = 0;
  }
  this->disposeDialog();
}

void MacOpenFileDialogContext::onNodeAttached()
{
  this->applyAttachedPresentation();
}

void MacOpenFileDialogContext::onNodeDetached()
{
  this->applyDetachedPresentation();
}

void MacOpenFileDialogContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                             loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (next == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
  else if (next == loka::app::scene::NODE_FACT_DETACHED_RETAINED)
  {
    this->applyDetachedPresentation();
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
  loka::core::MutableState<bool> *closeState = closeState_;
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  if (!panel)
  {
    dialog = this->detachDialogIfActive(dialog);
    if (!dialog)
    {
      return;
    }
    presentation_.markPresented();
    DeliverOpenFileDialogResult(resultState, onResult, closeState, loka::app::FileChooserResult::Error(1));
    delete dialog;
    return;
  }

  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseDirectories:NO];
  [panel setCanChooseFiles:YES];
  NSInteger response = [panel runModal];
  loka::app::FileChooserResult result = loka::app::FileChooserResult::Canceled();
#if defined(NSModalResponseOK)
  if (response == NSModalResponseOK)
#else
  if (response == NSOKButton)
#endif
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
  DeliverOpenFileDialogResult(resultState, onResult, closeState, result);
  delete dialog;
}

void MacOpenFileDialogContext::setResult(const loka::app::FileChooserResult &result)
{
  DeliverOpenFileDialogResult(resultState_, onResult_, closeState_, result);
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
