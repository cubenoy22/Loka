#include "MacOpenFileDialogContext.hpp"
#include "MacObjCCompat.hpp"
#include "Utf8String.hpp"
#import <AppKit/AppKit.h>

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
  if (visibleState_)
  {
    visibleState_->set(false);
  }
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
