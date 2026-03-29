#include "MacOpenFileDialogContext.hpp"
#include "../MacScenePlatformController.hpp"
#include "MacObjCCompat.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include "Utf8String.hpp"
#import <AppKit/AppKit.h>

namespace
{
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
}

MacOpenFileDialogContext::MacOpenFileDialogContext(void *parentView, loka::app::OpenFileDialogNode *node)
    : parentView_(parentView),
      node_(node),
      visibleState_(0),
      resultState_(0),
      onResult_(0),
      presenting_(false)
{
  visibleState_ = node_ ? node_->props.isVisible_ : 0;
  resultState_ = node_ ? node_->props.result_ : 0;
  onResult_ = node_ ? node_->props.onResult_ : 0;
  if (visibleState_)
  {
    bindVisible();
  }
}

MacOpenFileDialogContext::~MacOpenFileDialogContext()
{
  unbindVisible();
}

void MacOpenFileDialogContext::bindVisible()
{
  if (!visibleState_)
  {
    return;
  }
  visibleState_->deferBind(&MacOpenFileDialogContext::VisibleChangedThunk, this);
}

void MacOpenFileDialogContext::unbindVisible()
{
  if (!visibleState_)
  {
    return;
  }
  visibleState_->deferUnbind(&MacOpenFileDialogContext::VisibleChangedThunk, this);
}

void MacOpenFileDialogContext::applyVisible()
{
  if (!visibleState_)
  {
    return;
  }
  if (visibleState_->get())
  {
    // Consume the visible trigger first so duplicate listeners in the same
    // notification cycle do not re-open the dialog.
    visibleState_->set(false);
    presentDialog();
  }
}

void MacOpenFileDialogContext::presentDialog()
{
  if (presenting_)
  {
    return;
  }
  presenting_ = true;
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  if (!panel)
  {
    setResult(loka::app::FileChooserResult::Error(1));
    presenting_ = false;
    return;
  }

  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseDirectories:NO];
  [panel setCanChooseFiles:YES];
  NSInteger response = [panel runModal];
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
      setResult(loka::app::FileChooserResult::File(file));
    }
    else
    {
      setResult(loka::app::FileChooserResult::Error(2));
    }
  }
  else
  {
    setResult(loka::app::FileChooserResult::Canceled());
  }
  presenting_ = false;
}

void MacOpenFileDialogContext::setResult(const loka::app::FileChooserResult &result)
{
  if (resultState_)
  {
    resultState_->set(result, true);
  }
  if (onResult_)
  {
    onResult_->emit();
  }
}

void MacOpenFileDialogContext::VisibleChangedThunk(void *userData)
{
  MacOpenFileDialogContext *self = static_cast<MacOpenFileDialogContext *>(userData);
  if (self)
  {
    self->applyVisible();
  }
}

void RegisterMacOpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gMacOpenFileDialogNodeHandler);
}
