#include "context/ToolboxTextEditorContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "context/ToolboxPaintSupport.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include <Memory.h>
#include <algorithm>
#include <cstring>
using namespace loka::app;
namespace
{
  class Handler : public scene::RetainedNodeHandler<Handler, TextEditorNode, ToolboxTextEditorContext>
  {
  public:
    static TextEditorNode *cast(scene::Node *n)
    {
      return n && n->nodeTypeKey() == scene::NodeTypeToken<TextEditorNode>() ? static_cast<TextEditorNode *>(n) : 0;
    }
    static ToolboxTextEditorContext *
    create(TextEditorNode *n, scene::IPlatformController *c, const scene::LayoutState &)
    {
      return new ToolboxTextEditorContext(n, static_cast<ToolboxScenePlatformController *>(c));
    }
  };
  Handler handler;
  /** TE visual lineStarts include wrapped rows. Logical lines are CR-delimited.
      Borrow and lock the handle while scanning; copy only the selected slice. */
  class TextLock
  {
  public:
    explicit TextLock(TEHandle te)
        : handle_(reinterpret_cast<Handle>(TEGetText(te)))
    {
      if (this->handle_)
        HLock(this->handle_);
    }
    ~TextLock()
    {
      if (this->handle_)
        HUnlock(this->handle_);
    }
    const char *bytes() const
    {
      return this->handle_ ? *this->handle_ : 0;
    }

  private:
    TextLock(const TextLock &);
    TextLock &operator=(const TextLock &);
    Handle handle_;
  };
} // namespace
ToolboxTextEditorContext::ToolboxTextEditorContext(TextEditorNode *n, ToolboxScenePlatformController *c)
    : ToolboxProjectedNodeContext(c),
      node_(n),
      te_(0),
      restore_(static_cast<char *>(loka::core::LokaAllocRaw(
          TextEditorProps::kMaxBytes, loka::core::LokaAllocationSite("ToolboxTextEditor", "Restore")))),
      rect_(),
      paintRect_(),
      source_(0),
      caret_(),
      revision_(),
      phase_(IDLE),
      status_(EDITOR_UNAVAILABLE),
      restores_(0)
{
}
ToolboxTextEditorContext::~ToolboxTextEditorContext()
{
  assert(!this->te_ && "detach must revoke the borrowed TE before reclaim");
  loka::core::LokaFreeRaw(this->restore_, loka::core::LokaAllocationSite("ToolboxTextEditor", "Restore"));
}
void ToolboxTextEditorContext::invalidateNativePresentation()
{
  this->te_ = 0;
  this->source_ = 0;
  this->status_ = EDITOR_UNAVAILABLE;
}
void ToolboxTextEditorContext::onFactChanged(scene::NodeLifecycleFact previous, scene::NodeLifecycleFact next)
{
  if (next != scene::NODE_FACT_ATTACHED && this->controller())
    this->controller()->retireTextEditorControl(this, this->lifetimeHint());
  if (next == scene::NODE_FACT_RETIRED)
    this->node_ = 0;
  ToolboxProjectedNodeContext::onFactChanged(previous, next);
}
short ToolboxTextEditorContext::offsetOf(LineCursor cursor) const
{
  if (!this->node_ || !this->te_)
    return 0;
  const int index = this->node_->props.lines_->find(cursor.line);
  TextLock text(this->te_);
  if (!text.bytes())
    return 0;
  short start = 0;
  for (int row = 0; row < index && start < (**this->te_).teLength; ++start)
    if (text.bytes()[start] == '\r')
      ++row;
  short end = start;
  while (end < (**this->te_).teLength && text.bytes()[end] != '\r')
    ++end;
  return static_cast<short>(start + std::max(0, std::min(cursor.column, static_cast<int>(end - start))));
}
LineCursor ToolboxTextEditorContext::cursorAt(short offset) const
{
  if (!this->node_ || !this->te_ || !this->node_->props.lines_->size())
    return LineCursor::None();
  TextLock text(this->te_);
  if (!text.bytes())
    return LineCursor::None();
  unsigned short row = 0;
  short start = 0;
  offset = std::max(static_cast<short>(0), std::min(offset, (**this->te_).teLength));
  for (short i = 0; i < offset; ++i)
    if (text.bytes()[i] == '\r')
    {
      ++row;
      start = i + 1;
    }
  if (row >= this->node_->props.lines_->size())
    return LineCursor::None();
  return LineCursor(this->node_->props.lines_->at(row).id, offset - start);
}
void ToolboxTextEditorContext::project()
{
  if (!this->te_ || !this->node_ || this->phase_ != IDLE)
    return;
  this->phase_ = PROJECT;
  std::size_t length = 0;
  this->status_ = this->node_->document.project(this->restore_, TextEditorProps::kMaxBytes, length);
  if (this->status_ == EDITOR_OK)
  {
    const Rect dest = (**this->te_).destRect;
    TESetText(this->restore_, static_cast<long>(length), this->te_);
    (**this->te_).destRect = dest;
    // TESetText has no result and TE has no undo history. Do not advance the
    // projection fact until the native record confirms the complete payload.
    if ((**this->te_).teLength != static_cast<short>(length))
      this->status_ = EDITOR_UNAVAILABLE;
    else
    {
      this->caret_ = this->node_->props.cursor_.state()->get();
      if (this->node_->props.lines_->find(this->caret_.line) < 0)
        this->caret_ = this->node_->props.lines_->size()
                           ? LineCursor(this->node_->props.lines_->at(0).id, this->caret_.column)
                           : LineCursor::None();
      const short offset = this->offsetOf(this->caret_);
      TESetSelect(offset, offset, this->te_);
      this->caret_ = this->cursorAt(offset);
      this->source_ = this->node_->props.lines_;
      this->revision_ = this->source_->revision().get();
    }
  }
  this->phase_ = IDLE;
  if (this->controller() && this->controller()->window_)
    this->controller()->window_->requestInvalidateRect(this->paintRect_);
}
void ToolboxTextEditorContext::restoreCommittedProjection()
{
  assert(this->phase_ == IDLE);
  ++this->restores_;
  this->project();
}
void ToolboxTextEditorContext::retryProjection()
{
  // Called once by the foreground idle pass, outside scheduler drain callbacks.
  if (this->status_ != EDITOR_OK && this->te_)
    this->project();
}
void ToolboxTextEditorContext::onPropsApplied()
{
  if (!this->node_ || !this->te_ || this->phase_ != IDLE)
    return;
  if (this->source_ != this->node_->props.lines_ || this->revision_ != this->node_->props.lines_->revision().get())
    this->project();
  else if (this->status_ == EDITOR_OK && this->caret_ != this->node_->props.cursor_.state()->get())
  {
    const short offset = this->offsetOf(this->node_->props.cursor_.state()->get());
    TESetSelect(offset, offset, this->te_);
    this->caret_ = this->cursorAt(offset);
  }
}
EditorResult ToolboxTextEditorContext::beginInput()
{
  if (this->phase_ == PROJECT)
    return EDITOR_REENTRANT;
  if (this->phase_ != IDLE)
  {
    this->phase_ = RECONCILE;
    return EDITOR_REENTRANT;
  }
  if (!this->node_ || !this->te_)
    return EDITOR_UNAVAILABLE;
  if (this->status_ != EDITOR_OK)
    return this->status_;
  this->phase_ = INPUT;
  return EDITOR_OK;
}
EditorResult ToolboxTextEditorContext::finishInput(EditorResult result, Change change)
{
  const bool restore = result != EDITOR_OK || this->phase_ == RECONCILE;
  this->phase_ = IDLE;
  if (!this->te_ || !this->node_)
    return result;
  if (restore)
    this->restoreCommittedProjection();
  else
  {
    const loka::core::ListRevision after = this->node_->props.lines_->revision().get();
    // A commit publishes exactly one content revision (and one structure
    // revision for split/join). Extra owner writes are not our native edit.
    const bool ownerChanged = this->source_ != this->node_->props.lines_
                              || after.content != this->revision_.content + (change == CARET_CHANGE ? 0 : 1)
                              || after.structure != this->revision_.structure + (change == STRUCTURE_CHANGE ? 1 : 0);
    if (ownerChanged || this->cursorAt((**this->te_).selStart) != this->node_->props.cursor_.state()->get())
      this->project();
    else
    {
      this->caret_ = this->node_->props.cursor_.state()->get();
      this->revision_ = after;
    }
  }
  return result;
}
EditorResult ToolboxTextEditorContext::key(char key)
{
  EditorResult result = this->beginInput();
  if (result != EDITOR_OK)
    return result;
  const LineCursor before = this->caret_;
  Change change = LINE_CHANGE;
  const short start = (**this->te_).selStart, end = (**this->te_).selEnd;
  const short oldLength = (**this->te_).teLength;
  const Rect scroll = (**this->te_).destRect;
  const bool spansLines = start != end && this->cursorAt(start).line != this->cursorAt(end).line;
  TEKey(key, this->te_);
  if (key >= 28 && key <= 31)
  {
    change = CARET_CHANGE;
    result = this->node_->document.moveCaret(this->cursorAt((**this->te_).selStart));
  }
  else if (spansLines || (key == '\r' && start != end))
    result = EDITOR_INVALID_CURSOR; // Conservative refusal: cross-line selection seam is not in PR 1.
  else if (key == '\r' && start == end)
  {
    change = STRUCTURE_CHANGE;
    result = this->node_->document.applySplit(before.line, before.column);
  }
  else if (key == '\b' && before.column == 0 && start == end)
  {
    change = start ? STRUCTURE_CHANGE : CARET_CHANGE;
    result = start ? this->node_->document.applyJoin(before.line) : EDITOR_OK;
  }
  else
  {
    short first = (**this->te_).selStart, last = first;
    TextLock text(this->te_);
    if (!text.bytes())
      result = EDITOR_UNAVAILABLE;
    else
    {
      while (first > 0 && text.bytes()[first - 1] != '\r')
        --first;
      while (last < (**this->te_).teLength && text.bytes()[last] != '\r')
        ++last;
      if (last - first > TextEditorProps::kMaxBytes)
        result = EDITOR_CAPACITY;
      else
      {
        BlockMoveData(text.bytes() + first, this->restore_, last - first);
        if (key != '\b' && start == end && (**this->te_).teLength == oldLength + 1)
          result = this->node_->document.applyKeystroke(before, this->restore_ + start - first, 1);
        else
          result = this->node_->document.applySingleLine(before.line,
                                                         loka::core::String::Utf8(this->restore_, last - first),
                                                         LineCursor(before.line, (**this->te_).selStart - first));
      }
    }
  }
  const bool refused = result != EDITOR_OK || this->phase_ == RECONCILE;
  result = this->finishInput(result, change);
  if (refused && this->te_)
    (**this->te_).destRect = scroll;
  return result;
}
EditorResult ToolboxTextEditorContext::click(const Point &point)
{
  EditorResult result = this->beginInput();
  if (result != EDITOR_OK)
    return result;
  TEClick(point, false, this->te_);
  result = this->node_->document.moveCaret(this->cursorAt((**this->te_).selStart));
  return this->finishInput(result, CARET_CHANGE);
}
EditorResult ToolboxTextEditorContext::paste(const char *bytes, std::size_t length)
{
  EditorResult result = this->beginInput();
  if (result != EDITOR_OK)
    return result;
  // Refuse before TE's signed-short storage can overflow.
  if (length > TextEditorProps::kMaxBytes)
    result = EDITOR_CAPACITY;
  else if ((**this->te_).selStart != (**this->te_).selEnd)
    result = EDITOR_INVALID_CURSOR;
  else
    result = this->node_->document.applyKeystroke(this->caret_, bytes, length);
  result = this->finishInput(result, CARET_CHANGE);
  return result;
}
void ToolboxTextEditorContext::updateRect(const Rect &rect)
{
  this->paintRect_ = rect;
  if (this->controller() && !this->controller()->intersectWithProjectionClip(rect, this->paintRect_))
    SetRect(&this->paintRect_, 0, 0, 0, 0);
  if (this->te_)
  {
    if (!EqualRect(&rect, &this->rect_))
    {
      const short dx = (**this->te_).destRect.left - this->rect_.left;
      const short dy = (**this->te_).destRect.top - this->rect_.top;
      (**this->te_).destRect = rect;
      OffsetRect(&(**this->te_).destRect, dx, dy);
      TECalText(this->te_);
    }
    (**this->te_).viewRect = this->paintRect_;
  }
  this->rect_ = rect;
}
scene::PaintAnswer ToolboxTextEditorContext::queryPaintDamage(const scene::PaintQuery &query) const
{
  if (query.placement != scene::PLACEMENT_ELIGIBLE || query.scope != ToolboxPaintScope())
    return scene::PaintAnswer::refused(scene::PAINT_REFUSED_PLACEMENT_UNSETTLED);
  return scene::PaintAnswer::nativeScheduled();
}
short ToolboxTextEditorContext::layout(scene::IPlatformController *, scene::LayoutState &state)
{
  Rect rect;
  SetRect(&rect, state.x, state.y, state.x + state.width, state.y + (state.height > 0 ? state.height : 80));
  this->updateRect(rect);
  this->onPropsApplied();
  state.y = rect.bottom + state.spacing;
  return state.width;
}
void ToolboxTextEditorContext::repaint(TEHandle te)
{
  if (EmptyRect(&this->paintRect_))
    return;
  ToolboxPaintClip clip(this->paintRect_);
  if (clip.isActive() && !clip.touches(this->paintRect_))
    return;
  FrameRect(&this->rect_);
  if (te && this->status_ == EDITOR_OK)
    TEUpdate(&(**te).viewRect, te);
  else
  {
    EraseRect(&this->rect_);
    FrameRect(&this->rect_);
    MoveTo(this->rect_.left + 3, this->rect_.top + 14);
    const char label[] = "Editor unavailable";
    DrawText(label, 0, sizeof(label) - 1);
  }
}
void ToolboxTextEditorContext::render(scene::IPlatformController *)
{
  if (!this->controller() || !this->node_)
    return;
  TEHandle te = this->controller()->ensureTextEditorControl(this, this->rect_, this->lifetimeHint());
  if (te != this->te_)
  {
    this->te_ = te;
    this->project();
  }
  this->repaint(te);
}
bool RegisterToolboxTextEditorNodeHandler(scene::PlatformNodeHandlerRegistry &registry)
{
  return registry.registerHandler(&handler);
}
