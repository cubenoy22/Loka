#include "platform/null/context/NullTextEditorContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include <algorithm>
using namespace loka::app;
namespace
{
  class NullTextEditorHandler
      : public scene::RetainedNodeHandler<NullTextEditorHandler, TextEditorNode, NullTextEditorContext>
  {
  public:
    static TextEditorNode *cast(scene::Node *node)
    {
      return node && node->nodeTypeKey() == scene::NodeTypeToken<TextEditorNode>() ? static_cast<TextEditorNode *>(node)
                                                                                   : 0;
    }
    static NullTextEditorContext *create(TextEditorNode *node, scene::IPlatformController *, const scene::LayoutState &)
    {
      return new NullTextEditorContext(node);
    }
  };
  NullTextEditorHandler textEditorHandler;
} // namespace
NullTextEditorContext::NullTextEditorContext(TextEditorNode *node)
    : node_(node),
      buffer_(),
      caret_(),
      phase_(IDLE),
      status_(EDITOR_UNAVAILABLE),
      restores_(0)
{
  // Reserve cancellation capacity before accepting any input.
  this->buffer_.reserve(TextEditorProps::kMaxBytes);
}
void NullTextEditorContext::readLifecycleFactOnAttach()
{
  assert(this->node_ && this->node_->props.lines_);
  this->project(LineCursor::None());
  this->consumePendingRequest();
}
void NullTextEditorContext::project(LineCursor fallback)
{
  if (!this->node_ || this->node_->lifecycleFact() != scene::NODE_FACT_ATTACHED)
  {
    this->status_ = EDITOR_UNAVAILABLE;
    this->buffer_.clear();
    this->caret_ = LineCursor::None();
    return;
  }
  this->status_ = this->node_->document.availability();
  if (this->status_ != EDITOR_OK)
  {
    this->buffer_.clear();
    this->caret_ = LineCursor::None();
    return;
  }
  this->status_ = this->node_->document.project(this->buffer_);
  if (this->status_ != EDITOR_OK)
  {
    this->buffer_.clear();
    this->caret_ = LineCursor::None();
    return;
  }
  const loka::core::ObservableList<loka::core::String> &lines = *this->node_->props.lines_;
  this->caret_ = this->node_->props.cursorState()->get();
  if (lines.find(this->caret_.line) < 0)
  {
    this->caret_ = fallback;
    if (lines.find(this->caret_.line) < 0)
      this->caret_ = lines.size() ? LineCursor(lines.at(0).id, fallback.column) : LineCursor::None();
  }
  if (!this->caret_.isNone())
  {
    const int lineIndex = lines.find(this->caret_.line);
    std::size_t start = 0;
    for (int i = 0; i < lineIndex; ++i)
      start = this->buffer_.find('\r', start) + 1;
    const std::size_t end = this->buffer_.find('\r', start);
    const std::size_t length = (end == std::string::npos ? this->buffer_.size() : end) - start;
    this->caret_.column = std::max(0, std::min(this->caret_.column, static_cast<int>(length)));
  }
}
void NullTextEditorContext::restoreCommittedProjection(LineCursor snapshot)
{
  assert(this->phase_ == IDLE);
  ++this->restores_;
  this->project(snapshot);
}
std::size_t NullTextEditorContext::nativeOffset() const
{
  if (!this->node_)
    return 0;
  const int index = this->node_->props.lines_->find(this->caret_.line);
  std::size_t offset = 0;
  for (int i = 0; i < index; ++i)
  {
    const std::size_t end = this->buffer_.find('\r', offset);
    if (end == std::string::npos)
      break;
    offset = end + 1;
  }
  return std::min(this->buffer_.size(), offset + static_cast<std::size_t>(std::max(0, this->caret_.column)));
}
EditorResult NullTextEditorContext::input(const std::string &bytes, bool join, const LineCursor *move)
{
  if (!this->node_ || this->node_->lifecycleFact() != scene::NODE_FACT_ATTACHED)
  {
    this->consumePendingRequest();
    return EDITOR_UNAVAILABLE;
  }
  if (this->status_ != EDITOR_OK)
  {
    this->consumePendingRequest();
    return this->status_;
  }
  const LineCursor before = this->caret_;
  const std::size_t offset = this->nativeOffset();
  // A real native control has already changed before its owner is notified.
  if (move)
    this->caret_ = *move;
  else if (join)
  {
    if (offset)
      this->buffer_.erase(offset - 1, 1);
  }
  else
    this->buffer_.insert(offset, bytes);
  if (this->phase_ != IDLE)
  {
    this->phase_ = RECONCILE;
    return EDITOR_REENTRANT;
  }
  this->phase_ = INPUT;
  EditorResult result;
  if (move)
    result = this->node_->document.moveCaret(*move);
  else if (join)
  {
    const loka::core::ObservableList<loka::core::String> &lines = *this->node_->props.lines_;
    const int index = lines.find(before.line);
    if (before.column != 0)
      result = EDITOR_INVALID_CURSOR;
    else if (index < 0)
      result = EDITOR_STALE_ID;
    else if (!index)
      result = EDITOR_OK;
    else
    {
      const unsigned short previous = static_cast<unsigned short>(index - 1);
      const LineCursor from(lines.at(previous).id,
                            static_cast<LineCursor::Column>(
                                lines.at(previous).value.bufferWithEncoding(loka::core::StringEncodingUtf8).length()));
      result = this->node_->document.applyReplace(from, before, "", 0);
    }
  }
  else
    result = this->node_->document.applyReplace(before, before, bytes.data(), bytes.size());
  const bool reconcile = result != EDITOR_OK || this->phase_ == RECONCILE;
  this->phase_ = IDLE;
  if (reconcile)
    this->restoreCommittedProjection(before);
  else
    this->project(before);
  this->consumePendingRequest();
  return result;
}
void NullTextEditorContext::syncFromNode()
{
  if (this->phase_ == IDLE)
  {
    this->project(this->caret_);
    this->consumePendingRequest();
  }
}
void NullTextEditorContext::consumePendingRequest()
{
  if (this->phase_ != IDLE)
    return;
  while (this->node_)
  {
    const scene::WriteSeat<LineCursor> request = this->node_->props.moveCaretTo_;
    if (!request.isValid() || request.state()->get().isNone())
      return;
    this->phase_ = INPUT;
    LineCursor pending = request.state()->get();
    const TextEditorProps binding = this->node_->props;
    request.set(LineCursor::None());
    // Taking can notify app code. Recheck the borrowed binding before using it.
    if (this->node_ && this->phase_ == INPUT && this->node_->lifecycleFact() == scene::NODE_FACT_ATTACHED
        && this->node_->props.lines_ == binding.lines_ && this->node_->props.moveCaretTo_.state() == request.state()
        && this->node_->document.availability() == EDITOR_OK)
    {
      loka::core::StateTracker *owner = 0;
      if (binding.lines_->queryMutationTracker(owner) == loka::core::EDIT_OK && request.usesTracker(owner))
      {
        const int row = binding.lines_->find(pending.line);
        if (row >= 0)
        {
          std::size_t length = 0;
          if (binding.lines_->at(static_cast<unsigned short>(row))
                  .value.requiredUnits(loka::core::StringEncodingUtf8, length))
            pending.column = std::max(0, std::min(pending.column, static_cast<int>(length)));
        }
        this->node_->document.moveCaret(pending);
      }
    }
    // Reconcile any nested tentative input before delivering a report-time repost.
    this->phase_ = IDLE;
    this->project(this->caret_);
    // The outer completion is itself a delivery point. Re-read current Props:
    // report callbacks may have spent their dirty notification while INPUT.
  }
}
short NullTextEditorContext::layout(scene::IPlatformController *, scene::LayoutState &state)
{
  this->syncFromNode();
  // Fixed 80-pixel multiline block, preserving room for a following sibling.
  state.height = 80;
  return static_cast<short>(state.y + state.height + state.spacing);
}
void NullTextEditorContext::onFactChanged(scene::NodeLifecycleFact, scene::NodeLifecycleFact next)
{
  if (next != scene::NODE_FACT_ATTACHED)
  {
    if (this->phase_ != IDLE)
      this->phase_ = RECONCILE;
    if (next == scene::NODE_FACT_RETIRED)
      this->node_ = 0;
    this->buffer_.clear();
    this->caret_ = LineCursor::None();
    this->status_ = EDITOR_UNAVAILABLE;
  }
  else
    this->syncFromNode();
}
void RegisterNullTextEditorNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&textEditorHandler);
}

const void *NullTextEditorNodeHandlerKey()
{
  return textEditorHandler.nodeTypeKey();
}
bool IsNullTextEditorNodeHandler(const loka::app::scene::IPlatformNodeHandler *candidate)
{
  return candidate == &textEditorHandler;
}
