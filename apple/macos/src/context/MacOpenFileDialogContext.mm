#include "MacOpenFileDialogContext.hpp"
#import <AppKit/AppKit.h>

MacOpenFileDialogContext::MacOpenFileDialogContext(void *parentView, declara::app::OpenFileDialogNode *node)
    : parentView_(parentView), node_(node), visibleState_(0), lastVisible_(false), presenting_(false)
{
  visibleState_ = node_ ? node_->props.isVisible_ : 0;
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
  if (panel)
  {
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel runModal];
  }
  presenting_ = false;
}

void MacOpenFileDialogContext::VisibleChangedThunk(void *userData)
{
  MacOpenFileDialogContext *self = static_cast<MacOpenFileDialogContext *>(userData);
  if (self)
  {
    self->applyVisible();
  }
}
