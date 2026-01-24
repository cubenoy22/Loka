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

ToolboxOpenFileDialogContext::ToolboxOpenFileDialogContext(declara::app::OpenFileDialogNode *node)
    : node_(node),
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
  StandardGetFile(0, -1, 0, &reply);

  if (reply.sfGood)
  {
    setResult(declara::app::FileChooserResult::File(
        loka::file::platform::ItemAccess::FromFSSpec(
            reply.sfFile,
            loka::file::Item::KIND_FILE,
            displayPathFromSpec(reply.sfFile))));
  }
  else
  {
    setResult(declara::app::FileChooserResult::Canceled());
  }

  presenting_ = false;
}

void ToolboxOpenFileDialogContext::setResult(const declara::app::FileChooserResult &result)
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
