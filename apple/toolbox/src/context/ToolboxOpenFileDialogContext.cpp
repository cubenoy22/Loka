#include "context/ToolboxOpenFileDialogContext.hpp"
#include <StandardFile.h>

ToolboxOpenFileDialogContext::ToolboxOpenFileDialogContext(declara::app::OpenFileDialogNode *node)
    : node_(node), visibleState_(0), lastVisible_(false), presenting_(false)
{
  visibleState_ = node_ ? node_->props.isVisible_ : 0;
  if (visibleState_)
  {
    lastVisible_ = visibleState_->get();
    bindVisible();
    applyVisible();
  }
}

ToolboxOpenFileDialogContext::~ToolboxOpenFileDialogContext()
{
  unbindVisible();
}

void ToolboxOpenFileDialogContext::bindVisible()
{
  if (!visibleState_)
  {
    return;
  }
  visibleState_->deferBind(&ToolboxOpenFileDialogContext::VisibleChangedThunk, this);
}

void ToolboxOpenFileDialogContext::unbindVisible()
{
  if (!visibleState_)
  {
    return;
  }
  visibleState_->deferUnbind(&ToolboxOpenFileDialogContext::VisibleChangedThunk, this);
}

void ToolboxOpenFileDialogContext::applyVisible()
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

void ToolboxOpenFileDialogContext::presentDialog()
{
  if (presenting_)
  {
    return;
  }
  presenting_ = true;

  StandardFileReply reply;
  StandardGetFile(0, 0, 0, &reply);

  presenting_ = false;
}

void ToolboxOpenFileDialogContext::VisibleChangedThunk(void *userData)
{
  ToolboxOpenFileDialogContext *self = static_cast<ToolboxOpenFileDialogContext *>(userData);
  if (self)
  {
    self->applyVisible();
  }
}
