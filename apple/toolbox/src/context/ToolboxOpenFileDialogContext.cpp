#include "context/ToolboxOpenFileDialogContext.hpp"
#include <StandardFile.h>
#include <string>

static loka::core::String displayPathFromSpec(const FSSpec &spec)
{
  char name[64];
  const unsigned char length = spec.name[0];
  const unsigned char capped = length > 63 ? 63 : length;
  for (unsigned char i = 0; i < capped; ++i)
  {
    name[i] = static_cast<char>(spec.name[i + 1]);
  }
  name[capped] = '\0';
  return loka::core::String(std::string(name, capped));
}

ToolboxOpenFileDialogContext::ToolboxOpenFileDialogContext(loka::app::OpenFileDialogNode *node)
    : node_(node),
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
  if (visibleState_->get())
  {
    presentDialog();
  }
}

void ToolboxOpenFileDialogContext::presentDialog()
{
  if (presenting_)
  {
    return;
  }
  presenting_ = true;

  StandardFileReply reply;
  StandardGetFile(0, -1, 0, &reply);

  if (reply.sfGood)
  {
    loka::file::File file(displayPathFromSpec(reply.sfFile));
    file.setKind(loka::file::File::KIND_FILE);
    setResult(loka::app::FileChooserResult::File(file));
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

void ToolboxOpenFileDialogContext::setResult(const loka::app::FileChooserResult &result)
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

void ToolboxOpenFileDialogContext::VisibleChangedThunk(void *userData)
{
  ToolboxOpenFileDialogContext *self = static_cast<ToolboxOpenFileDialogContext *>(userData);
  if (self)
  {
    self->applyVisible();
  }
}
