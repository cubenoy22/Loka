#include "context/ToolboxOpenFileDialogContext.hpp"
#include "ToolboxFileChoice.hpp"
#include "platform/ToolboxPascalText.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include <StandardFile.h>
#include <string>

namespace
{
  struct ToolboxOpenNativeDialogSession
  {
    ToolboxOpenNativeDialogSession()
        : disposed(false)
    {
    }

    bool disposed;
  };

  static void DeliverOpenFileDialogResult(loka::app::scene::NodeState<loka::app::FileChooserResult> resultState,
                                          loka::core::EmitterState *onResult,
                                          const loka::app::FileChooserResult &result)
  {
    void *onResultToken = onResult ? onResult->retainExternalLifetimeToken() : 0;
    if (resultState.isValid())
    {
      resultState.set(result, true);
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

  class ToolboxOpenFileDialogNodeHandler
      : public loka::app::scene::RetainedNodeHandler<ToolboxOpenFileDialogNodeHandler,
                                                     loka::app::OpenFileDialogNode,
                                                     ToolboxOpenFileDialogContext>
  {
  public:
    static loka::app::OpenFileDialogNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asOpenFileDialogNode() : 0;
    }

    static ToolboxOpenFileDialogContext *create(loka::app::OpenFileDialogNode *node,
                                                loka::app::scene::IPlatformController *controller,
                                                const loka::app::scene::LayoutState &state)
    {
      ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
      (void)state;
      // Keep the installation ordering at the shared ritual; see RetainedNodeHandler.
      return new ToolboxOpenFileDialogContext(node, toolbox ? toolbox->cursorOwner() : 0);
    }
  };

  ToolboxOpenFileDialogNodeHandler gToolboxOpenFileDialogNodeHandler;
} // namespace

struct ToolboxOpenFileDialogContext::NativeDialogSession : public ToolboxOpenNativeDialogSession
{
};

ToolboxOpenFileDialogContext::ToolboxOpenFileDialogContext(loka::app::OpenFileDialogNode *node, CursorOwner *cursorOwner)
    : cursorOwner_(cursorOwner), node_(node),
      props_(),
      presentation_(),
      dialog_(0)
{
  this->captureProps();
}

ToolboxOpenFileDialogContext::~ToolboxOpenFileDialogContext()
{
  this->disposeDialog();
}

void ToolboxOpenFileDialogContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void ToolboxOpenFileDialogContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
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
  }
}

void ToolboxOpenFileDialogContext::applyAttachedPresentation()
{
  presentIfNeeded();
}

void ToolboxOpenFileDialogContext::applyDetachedPresentation()
{
  presentation_.markDetached();
  this->disposeDialog();
}

void ToolboxOpenFileDialogContext::presentIfNeeded()
{
  if (dialog_ || !presentation_.beginPresent())
  {
    return;
  }
  dialog_ = new NativeDialogSession();
  presentDialog();
}

void ToolboxOpenFileDialogContext::presentDialog()
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
  loka::app::scene::NodeState<loka::app::FileChooserResult> resultState = this->props_.result_;
  loka::core::EmitterState *onResult = this->props_.onResult_;

  const loka::app::FileDialogOptions options = this->props_.options_;
  StandardFileReply reply;
  reply.sfGood = false;
  loka::app::FileChooserResult result = loka::app::FileChooserResult::Canceled();
  switch (options.purpose())
  {
  case loka::app::FILE_DIALOG_OPEN:
    // Both policies admit all files on Toolbox, preserving the OPEN behavior.
    StandardGetFile(0, -1, 0, &reply);
    break;
  case loka::app::FILE_DIALOG_SAVE:
  {
    const unsigned char prompt[] = {8, 'S', 'a', 'v', 'e', ' ', 'a', 's', ':'};
    Str255 defaultName;
    loka::core::String roundTrip;
    // The short-label door can substitute or cap text. A filename must survive
    // its round trip exactly, in addition to fitting the HFS byte limit.
    if (!ToolboxEncodePascal(options.defaultName(), defaultName)
        || defaultName[0] > 31
        || !ToolboxDecodeNative(defaultName + 1, defaultName[0], roundTrip)
        || !roundTrip.equals(options.defaultName()))
    {
      result = loka::app::FileChooserResult::Error(paramErr);
      break;
    }
    StandardPutFile(prompt, defaultName, &reply);
    break;
  }
  }
  if (this->cursorOwner_)
    this->cursorOwner_->reconcile();

  if (reply.sfGood)
  {
    if (options.purpose() == loka::app::FILE_DIALOG_SAVE
        && (reply.sfFile.name[0] == 0 || reply.sfFile.name[0] > 31))
      result = loka::app::FileChooserResult::Error(paramErr);
    else
    {
      loka::file::File file;
      result = ToolboxCaptureChosenFile(reply.sfFile, file)
          ? loka::app::FileChooserResult::File(file)
          : loka::app::FileChooserResult::Error(memFullErr);
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

void ToolboxOpenFileDialogContext::setResult(const loka::app::FileChooserResult &result)
{
  DeliverOpenFileDialogResult(this->props_.result_, this->props_.onResult_, result);
}

void ToolboxOpenFileDialogContext::disposeDialog()
{
  if (!dialog_)
  {
    return;
  }
  dialog_->disposed = true;
  delete dialog_;
  dialog_ = 0;
}

ToolboxOpenFileDialogContext::NativeDialogSession *
ToolboxOpenFileDialogContext::detachDialogIfActive(NativeDialogSession *dialog)
{
  if (!dialog || dialog_ != dialog || dialog->disposed)
  {
    return 0;
  }
  dialog_ = 0;
  dialog->disposed = true;
  return dialog;
}

bool RegisterToolboxOpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  return registry.registerHandler(&gToolboxOpenFileDialogNodeHandler);
}

void ToolboxOpenFileDialogContext::captureProps()
{
  this->props_ = this->node_ ? this->node_->props : loka::app::OpenFileDialogProps();
}

void ToolboxOpenFileDialogContext::onPropsApplied()
{
  if (this->presentation_.isPresenting() && this->node_
      && (this->props_ < this->node_->props || this->node_->props < this->props_))
  {
    this->disposeDialog();
    this->presentation_.markPresented(); // Retarget abandons until reattach.
  }
  this->captureProps();
}
