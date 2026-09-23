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
  this->settle(scene::SETTLE_ATTACH,
               this->node_ && this->node_->props.cursorState() ? this->node_->props.cursorState()->get()
                                                               : LineCursor::None());
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
    this->settle(scene::SETTLE_INPUT,
                 this->node_ && this->node_->props.cursorState() ? this->node_->props.cursorState()->get()
                                                                 : LineCursor::None());
    return EDITOR_UNAVAILABLE;
  }
  if (this->status_ != EDITOR_OK)
  {
    const EditorResult status = this->status_;
    this->settle(scene::SETTLE_INPUT,
                 this->node_ && this->node_->props.cursorState() ? this->node_->props.cursorState()->get()
                                                                 : LineCursor::None());
    return status;
  }
  const LineCursor factBefore = this->node_->props.cursorState()->get();
  scene::Node *const liveNode = this->node_;
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
  if (liveNode->getContext() != this)
    return result;
  const bool reconcile = result != EDITOR_OK || this->phase_ == RECONCILE;
  this->phase_ = IDLE;
  if (reconcile)
    this->restoreCommittedProjection(before);
  else
    this->project(before);
  this->settle(scene::SETTLE_INPUT, factBefore);
  return result;
}
void NullTextEditorContext::syncFromNode()
{
  if (this->phase_ == IDLE)
  {
    this->project(this->caret_);
    this->settle(scene::SETTLE_PROPS,
                 this->node_ && this->node_->props.cursorState() ? this->node_->props.cursorState()->get()
                                                                 : LineCursor::None());
  }
}
/** Stack policy: context lookup occurs only after the driver's lifetime wall. */
class NullTextEditorContext::RailOperation : public scene::RailOperation<LineCursor>
{
public:
  virtual scene::Admission admit(scene::Node &base, scene::RequestBinding<LineCursor> &request)
  {
    NullTextEditorContext &c = context(base);
    request = static_cast<TextEditorNode &>(base).props.moveCaretTo_;
    this->binding_ = static_cast<TextEditorNode &>(base).props;
    if (c.phase_ != IDLE || !c.node_)
      return scene::ADMISSION_DEFERRED;
    if (!request.isValid() || request.state()->get().isNone())
      return scene::ADMISSION_EMPTY;
    c.phase_ = INPUT;
    return scene::ADMISSION_TAKE;
  }
  virtual bool current(scene::Node &base, const scene::RequestBinding<LineCursor> &request)
  {
    TextEditorNode &node = static_cast<TextEditorNode &>(base);
    return node.lifecycleFact() == scene::NODE_FACT_ATTACHED && node.props.lines_ == this->binding_.lines_
           && node.props.cursorState() == this->binding_.cursorState() && node.props.moveCaretTo_.same(request);
  }
  virtual EditorResult resolve(scene::Node &base, const scene::RequestBinding<LineCursor> &request)
  {
    if (!this->current(base, request))
      return EDITOR_OWNER_MISMATCH;
    if (context(base).phase_ != INPUT)
      return EDITOR_REENTRANT;
    loka::core::StateTracker *owner = 0;
    if (!this->binding_.lines_ || this->binding_.lines_->queryMutationTracker(owner) != loka::core::EDIT_OK)
      return EDITOR_UNAVAILABLE;
    return request.usesTracker(owner) ? EDITOR_OK : EDITOR_OWNER_MISMATCH;
  }
  virtual EditorResult validate(scene::Node &base, const LineCursor &pending)
  {
    TextEditorNode &node = static_cast<TextEditorNode &>(base);
    const EditorResult ready = node.document.availability();
    if (ready != EDITOR_OK)
      return ready;
    return node.props.lines_->find(pending.line) < 0 ? EDITOR_STALE_ID : EDITOR_OK;
  }
  virtual scene::RequestApplication<LineCursor> apply(scene::Node &base, const LineCursor &pending)
  {
    TextEditorNode &node = static_cast<TextEditorNode &>(base);
    const int row = node.props.lines_->find(pending.line);
    std::size_t length = 0;
    if (row < 0)
      return scene::RequestApplication<LineCursor>(pending, EDITOR_STALE_ID);
    if (!node.props.lines_->at(static_cast<unsigned short>(row))
             .value.requiredUnits(loka::core::StringEncodingUtf8, length))
      return scene::RequestApplication<LineCursor>(pending, EDITOR_UNAVAILABLE);
    const LineCursor applied(pending.line, std::max(0, std::min(pending.column, static_cast<int>(length))));
    return scene::RequestApplication<LineCursor>(applied, EDITOR_OK);
  }
  virtual EditorResult report(scene::Node &base, const LineCursor &applied)
  {
    return static_cast<TextEditorNode &>(base).document.moveCaret(applied);
  }
  virtual scene::FollowUp finishTake(scene::Node &base, const scene::Reply<LineCursor> &,
                                    const scene::RequestApplication<LineCursor> &)
  {
    NullTextEditorContext &c = context(base);
    c.phase_ = IDLE;
    c.project(c.caret_);
    // Null writes its simulated projection here, including refused-take repair.
    return scene::REPAINT;
  }
  // Null controls are native-scheduled paint answers; there is no repaint sink.
  virtual scene::FollowUpResult finishSettle(scene::Node &, const scene::FollowUps &)
  {
    return scene::FOLLOW_UP_NONE;
  }
#ifdef TEST_BUILD
  virtual LineCursor fact(scene::Node &base) const
  {
    loka::core::State<LineCursor> *state = static_cast<TextEditorNode &>(base).props.cursorState();
    return state ? state->get() : LineCursor::None();
  }
#endif
private:
  static NullTextEditorContext &context(scene::Node &node)
  {
    return *static_cast<NullTextEditorContext *>(node.getContext());
  }
  TextEditorProps binding_;
};
void NullTextEditorContext::settle(scene::Settlement stimulus
#ifdef TEST_BUILD
                                   ,
                                   LineCursor before
#endif
)
{
  RailOperation op;
  scene::RequestSettlement<LineCursor>::settle(this->node_,
                                               this,
                                               op,
                                               stimulus
#ifdef TEST_BUILD
                                               ,
                                               before
#endif
  );
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
