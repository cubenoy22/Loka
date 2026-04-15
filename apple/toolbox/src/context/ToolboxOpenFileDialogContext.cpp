#include "context/ToolboxOpenFileDialogContext.hpp"
#include "ToolboxPlatformContext.hpp"
#include <StandardFile.h>
#include <string>

namespace
{
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
}

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
      resultState_(0),
      onResult_(0),
      presentation_()
{
  resultState_ = node_ ? node_->props.result_ : 0;
  onResult_ = node_ ? node_->props.onResult_ : 0;
}

ToolboxOpenFileDialogContext::~ToolboxOpenFileDialogContext()
{
}

void ToolboxOpenFileDialogContext::onNodeAttached()
{
  presentIfNeeded();
}

void ToolboxOpenFileDialogContext::onNodeDetached()
{
  presentation_.markDetached();
}

void ToolboxOpenFileDialogContext::presentIfNeeded()
{
  if (!presentation_.beginPresent())
  {
    return;
  }
  presentDialog();
}

void ToolboxOpenFileDialogContext::presentDialog()
{
  if (!presentation_.isPresenting())
  {
    return;
  }
  loka::core::MutableState<loka::app::FileChooserResult> *resultState = resultState_;
  loka::core::EmitterState *onResult = onResult_;

  StandardFileReply reply;
  StandardGetFile(0, -1, 0, &reply);
  loka::app::FileChooserResult result = loka::app::FileChooserResult::Canceled();

  if (reply.sfGood)
  {
    const loka::core::String displayPath = displayPathFromSpec(reply.sfFile);
    loka::file::File file(displayPath);
    file.setKind(loka::file::File::KIND_FILE);
#if defined(LOKA_RETRO68)
    ToolboxPlatformContext::registerChosenFileSpec(displayPath, reply.sfFile);
#endif
    result = loka::app::FileChooserResult::File(file);
  }

  presentation_.markPresented();
  DeliverOpenFileDialogResult(resultState, onResult, result);
}

void ToolboxOpenFileDialogContext::setResult(const loka::app::FileChooserResult &result)
{
  DeliverOpenFileDialogResult(resultState_, onResult_, result);
}
