#include "context/ToolboxOpenFileDialogContext.hpp"
#include "ToolboxFileChoice.hpp"
#include "platform/ToolboxPascalText.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include <StandardFile.h>
#include <cassert>

void DeliverOpenFileDialogResult(loka::app::scene::NodeState<loka::app::FileChooserResult> resultState,
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

namespace
{
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
      return new ToolboxOpenFileDialogContext(node, toolbox ? &toolbox->pendingDialogs() : 0);
    }
  };

  ToolboxOpenFileDialogNodeHandler gToolboxOpenFileDialogNodeHandler;
} // namespace

ToolboxOpenFileDialogContext::ToolboxOpenFileDialogContext(loka::app::OpenFileDialogNode *node,
                                                           ToolboxPendingDialogs *pending)
    : pending_(pending), enrollment_(this), node_(node), props_(), presentation_()
{
  this->captureProps();
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
    // Both retained detach and terminal retirement cancel pending presentation.
    this->applyDetachedPresentation();
  }
}

void ToolboxOpenFileDialogContext::applyAttachedPresentation()
{
  if (this->pending_
      && this->presentation_.value == loka::app::OPEN_FILE_DIALOG_PRESENTATION_PENDING_ATTACH)
    this->pending_->enroll(this->enrollment_);
}

void ToolboxOpenFileDialogContext::applyDetachedPresentation()
{
  this->enrollment_.unlink();
  this->presentation_.markDetached();
}

loka::app::FileChooserResult RunToolboxFileDialog(const loka::app::FileDialogOptions &options)
{
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

  return result;
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
  this->captureProps();
}

bool ToolboxOpenFileDialogContext::take(loka::app::OpenFileDialogProps &out)
{
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return false;
  this->enrollment_.unlink();
  out = this->props_;
  this->presentation_.markPresented();
  return true;
}

ToolboxDialogEnrollment::ToolboxDialogEnrollment(ToolboxOpenFileDialogContext *owner)
    : prev_(0), next_(0), owner_(owner)
{
}

ToolboxDialogEnrollment::~ToolboxDialogEnrollment()
{
  this->unlink();
}

void ToolboxDialogEnrollment::unlink()
{
  if (!this->next_)
    return;
  this->prev_->next_ = this->next_;
  this->next_->prev_ = this->prev_;
  this->prev_ = this->next_ = 0;
}

ToolboxPendingDialogs::ToolboxPendingDialogs() : sentinel_(0)
{
  this->sentinel_.prev_ = this->sentinel_.next_ = &this->sentinel_;
}

ToolboxPendingDialogs::~ToolboxPendingDialogs()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const bool wasEmpty = this->sentinel_.next_ == &this->sentinel_;
#endif
  while (this->sentinel_.next_ != &this->sentinel_)
    this->sentinel_.next_->unlink();
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(wasEmpty && "pending dialogs must detach before controller teardown");
  (void)wasEmpty;
#endif
}

void ToolboxPendingDialogs::enroll(ToolboxDialogEnrollment &row)
{
  if (row.next_)
    return;
  row.prev_ = this->sentinel_.prev_;
  row.next_ = &this->sentinel_;
  row.prev_->next_ = &row;
  this->sentinel_.prev_ = &row;
}

bool ToolboxPendingDialogs::take(loka::app::OpenFileDialogProps &out)
{
  if (this->sentinel_.next_ == &this->sentinel_)
    return false;
  return this->sentinel_.next_->owner_->take(out);
}
