#include "MacOpenFileDialogContext.hpp"
#include "Utf8String.hpp"
#import <AppKit/AppKit.h>

MacOpenFileDialogContext::MacOpenFileDialogContext(void *parentView, declara::app::OpenFileDialogNode *node)
    : parentView_(parentView),
      node_(node),
      visibleState_(0),
      resultState_(0),
      onResult_(0),
      lastVisible_(false),
      presenting_(false)
{
  visibleState_ = node_ ? node_->props.isVisible_ : 0;
  resultState_ = node_ ? node_->props.result_ : 0;
  onResult_ = node_ ? node_->props.onResult_ : 0;
  if (visibleState_)
  {
    lastVisible_ = visibleState_->get();
    bindVisible();
    applyVisible();
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
  const bool visible = visibleState_->get();
  if (visible && !lastVisible_)
  {
    presentDialog();
  }
  lastVisible_ = visible;
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
    setResult(declara::app::FileChooserResult::Error(1));
    presenting_ = false;
    return;
  }

  [panel setAllowsMultipleSelection:NO];
  [panel setCanChooseDirectories:NO];
  [panel setCanChooseFiles:YES];
  NSModalResponse response = [panel runModal];
  if (response == NSModalResponseOK)
  {
    NSURL *url = [panel URL];
    if (url)
    {
      std::string path = declara::macos::Utf8FromNSString([url path]);
      setResult(declara::app::FileChooserResult::File(
          declara::app::FileRef::FromPath(loka::core::String(path))));
    }
    else
    {
      setResult(declara::app::FileChooserResult::Error(2));
    }
  }
  else
  {
    setResult(declara::app::FileChooserResult::Canceled());
  }
  presenting_ = false;
}

void MacOpenFileDialogContext::setResult(const declara::app::FileChooserResult &result)
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
