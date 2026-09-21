#include "Win32TextEditorContext.hpp"
#include "Win32EditTextBridge.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include <cassert>

using namespace loka::app;
namespace
{
  const wchar_t kEditorContext[] = L"Loka.TextEditor.Context";
  const UINT_PTR kRestoreTimer = 853;
  class TextEditorHandler : public scene::RetainedNodeHandler<TextEditorHandler, TextEditorNode, Win32TextEditorContext>
  {
  public:
    static TextEditorNode *cast(scene::Node *node)
    {
      return node && node->nodeTypeKey() == scene::NodeTypeToken<TextEditorNode>() ? static_cast<TextEditorNode *>(node)
                                                                                   : 0;
    }
    static Win32TextEditorContext *
    create(TextEditorNode *node, scene::IPlatformController *controller, const scene::LayoutState &state)
    {
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      return new Win32TextEditorContext(win32, win32->projectionParentHwnd(), state, node);
    }
    static void refresh(Win32TextEditorContext *context, const scene::LayoutState &state)
    {
      context->onPropsApplied();
      context->relayout(state);
    }
  };
  TextEditorHandler editorHandler;
  bool isInputMessage(UINT message)
  {
    return message == WM_CHAR || message == WM_KEYDOWN || message == WM_KEYUP || message == WM_LBUTTONDOWN
           || message == WM_LBUTTONUP || message == WM_LBUTTONDBLCLK || message == WM_MOUSEMOVE || message == WM_CUT
           || message == WM_CLEAR || message == WM_PASTE || message == WM_UNDO || message == EM_UNDO
           || message == EM_REPLACESEL;
  }
} // namespace

Win32TextEditorContext::Projection::Projection()
    : owner(0),
      revision(),
      text(),
      wide()
{
  this->text.reserve(TextEditorProps::kMaxBytes);
  this->wide.reserve(TextEditorProps::kMaxBytes + TextEditorProps::kMaxLines);
}
bool Win32TextEditorContext::Projection::current(const TextEditorNode &node) const
{
  if (!this->owner || this->owner != node.props.lines_ || this->revision != this->owner->revision().get())
    return false;
  loka::core::StateTracker *tracker = 0;
  const loka::core::ListEditResult ready = this->owner->queryMutationTracker(tracker);
  return (ready == loka::core::EDIT_OK || ready == loka::core::EDIT_REENTRANT) && node.props.cursor_.isValid()
         && node.props.cursor_.usesTracker(tracker);
}
EditorResult Win32TextEditorContext::Projection::capture(TextEditorNode &node)
{
  if (this->current(node))
    return EDITOR_OK;
  this->owner = 0;
  const EditorResult result = node.document.project(this->text);
  if (result != EDITOR_OK || !loka::win32::TextEditorToWide(this->text, this->wide))
  {
    this->text.clear();
    this->wide.clear();
    return result == EDITOR_OK ? EDITOR_NON_ASCII : result;
  }
  this->owner = node.props.lines_;
  this->revision = this->owner->revision().get();
  return EDITOR_OK;
}

Win32TextEditorContext::Win32TextEditorContext(Win32ScenePlatformController *controller,
                                               HWND parent,
                                               const scene::LayoutState &state,
                                               TextEditorNode *node)
    : Win32RetirableContext(controller),
      node_(node),
      hwnd_(0),
      previousProc_(0),
      phase_(IDLE),
      status_(EDITOR_UNAVAILABLE),
      selection_(),
      projection_(),
      restores_(0),
      delivery_(scene::PaintAnswer::refused(scene::PAINT_REFUSED_HISTORY_UNKNOWN))
{
  this->hwnd_ = this->createNativeChildWindow(
      loka::win32::EditTextControlExStyle(),
      L"EDIT",
      L"",
      loka::win32::EditTextControlStyle() | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL,
      controller->displayScale().projectFrame(loka::core::Frame(state.x, state.y, state.width, state.height)),
      parent,
      NULL,
      GetModuleHandleW(NULL),
      NULL);
  if (this->hwnd_)
  {
    if (!SetPropW(this->hwnd_, kEditorContext, this))
    {
      this->retireWindow(this->hwnd_);
      return;
    }
    this->previousProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(this->hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc)));
    if (!this->previousProc_)
    {
      RemovePropW(this->hwnd_, kEditorContext);
      this->retireWindow(this->hwnd_);
      return;
    }
    // The owner enforces the logical cap. Leave room for an over-cap native
    // action so it is refused atomically instead of accepting a truncated paste.
    SendMessageW(this->hwnd_, EM_SETLIMITTEXT, 0, 0);
  }
}
Win32TextEditorContext::~Win32TextEditorContext()
{
  assert(!this->hwnd_ && "terminal lifecycle must strip the native input route before reclaim");
}
Win32TextEditorContext *Win32TextEditorContext::fromWindow(HWND window)
{
  return static_cast<Win32TextEditorContext *>(GetPropW(window, kEditorContext));
}
void Win32TextEditorContext::readLifecycleFactOnAttach()
{
  this->syncFromNode();
}
void Win32TextEditorContext::onPropsApplied()
{
  if (this->phase_ == IDLE || this->phase_ == RETRY)
    this->syncFromNode();
}
void Win32TextEditorContext::onFactChanged(scene::NodeLifecycleFact, scene::NodeLifecycleFact next)
{
  if (!this->hwnd_)
    return;
  if (next == scene::NODE_FACT_ATTACHED)
  {
    this->syncFromNode();
    ShowWindow(this->hwnd_, SW_SHOW);
    return;
  }
  KillTimer(this->hwnd_, kRestoreTimer);
  ShowWindow(this->hwnd_, SW_HIDE);
  SendMessageW(this->hwnd_, EM_SETREADONLY, TRUE, 0);
  this->status_ = EDITOR_UNAVAILABLE;
  this->phase_ = this->phase_ == INPUT || this->phase_ == COMMIT ? REJECTED : IDLE;
  this->delivery_ = scene::PaintAnswer::refused(scene::PAINT_REFUSED_HISTORY_UNKNOWN);
  if (next == scene::NODE_FACT_RETIRED)
  {
    RemovePropW(this->hwnd_, kEditorContext);
    SetWindowLongPtrW(this->hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(this->previousProc_));
    this->retireWindow(this->hwnd_);
    this->node_ = 0;
  }
}
void Win32TextEditorContext::captureSelection()
{
  SendMessageW(this->hwnd_,
               EM_GETSEL,
               reinterpret_cast<WPARAM>(&this->selection_.start),
               reinterpret_cast<LPARAM>(&this->selection_.end));
  this->selection_.firstLine = static_cast<int>(SendMessageW(this->hwnd_, EM_LINEFROMCHAR, this->selection_.start, 0));
  this->selection_.lastLine = static_cast<int>(SendMessageW(this->hwnd_, EM_LINEFROMCHAR, this->selection_.end, 0));
  this->selection_.firstVisible = static_cast<int>(SendMessageW(this->hwnd_, EM_GETFIRSTVISIBLELINE, 0, 0));
  this->selection_.horizontal = GetScrollPos(this->hwnd_, SB_HORZ);
}
void Win32TextEditorContext::deferRestore()
{
  if (!this->hwnd_ || !this->node_ || this->node_->lifecycleFact() != scene::NODE_FACT_ATTACHED)
    return;
  this->phase_ = RETRY;
  SendMessageW(this->hwnd_, EM_SETREADONLY, TRUE, 0);
  // A window timer is a later native turn, not the StateTracker drain loop.
  // Failure to arm leaves the explicit read-only state; props application can retry.
  SetTimer(this->hwnd_, kRestoreTimer, 1, NULL);
}
bool Win32TextEditorContext::replaceProjection()
{
  if (!this->hwnd_ || !this->node_ || this->node_->lifecycleFact() != scene::NODE_FACT_ATTACHED)
    return false;
  this->phase_ = RESTORING;
  const EditorResult projected = this->projection_.capture(*this->node_);
  const bool available = projected == EDITOR_OK;
  // Deliberately bypass State equality and the ordinary projection cache.
  const bool submitted = SetWindowTextW(this->hwnd_, this->projection_.wide.c_str()) != FALSE;
  const bool complete =
      submitted && GetWindowTextLengthW(this->hwnd_) == static_cast<int>(this->projection_.wide.size());
  SendMessageW(this->hwnd_, EM_EMPTYUNDOBUFFER, 0, 0);
  if (!complete)
  {
    this->status_ = EDITOR_UNAVAILABLE;
    this->delivery_ = scene::PaintAnswer::refused(scene::PAINT_REFUSED_PROPS_UNRECONCILED);
    this->deferRestore();
    return false;
  }
  this->restoreSelection();
  this->status_ = projected;
  this->phase_ = IDLE;
  SendMessageW(this->hwnd_, EM_SETREADONLY, available ? FALSE : TRUE, 0);
  this->delivery_ = scene::PaintAnswer::nativeScheduled();
  if (projected == EDITOR_ALLOCATION)
    this->deferRestore();
  return true;
}
void Win32TextEditorContext::restoreSelection()
{
  DWORD start = this->selection_.start, end = this->selection_.end;
  if (this->node_->props.lines_ && this->node_->props.cursor_.isValid())
  {
    const LineCursor cursor = this->node_->props.cursor_.state()->get();
    const int index = this->node_->props.lines_->find(cursor.line);
    if (index >= 0)
    {
      const int offset = static_cast<int>(SendMessageW(this->hwnd_, EM_LINEINDEX, index, 0));
      const int length = static_cast<int>(SendMessageW(this->hwnd_, EM_LINELENGTH, offset, 0));
      const int column = cursor.column < 0 ? 0 : (cursor.column > length ? length : cursor.column);
      start = end = static_cast<DWORD>(offset + column);
    }
  }
  SendMessageW(this->hwnd_, EM_SETSEL, start, end);
  const int visible = static_cast<int>(SendMessageW(this->hwnd_, EM_GETFIRSTVISIBLELINE, 0, 0));
  SendMessageW(this->hwnd_, EM_LINESCROLL, 0, this->selection_.firstVisible - visible);
  SendMessageW(this->hwnd_, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, this->selection_.horizontal), 0);
}
void Win32TextEditorContext::restoreCommittedProjection()
{
  assert(this->phase_ != COMMIT && this->phase_ != RESTORING);
  ++this->restores_;
  this->replaceProjection();
}
void Win32TextEditorContext::syncFromNode()
{
  if (this->phase_ != IDLE && this->phase_ != RETRY)
    return;
  if (!this->hwnd_ || !this->node_ || this->node_->lifecycleFact() != scene::NODE_FACT_ATTACHED)
    return;
  if (this->phase_ == RETRY)
  {
    KillTimer(this->hwnd_, kRestoreTimer);
    this->restoreCommittedProjection();
    return;
  }
  this->captureSelection();
  if (this->status_ == EDITOR_OK && this->projection_.current(*this->node_))
  {
    // A native echo must preserve selection and undo. Only an externally
    // moved committed cursor collapses the native selection.
    const LineCursor desired = this->node_->props.cursor_.state()->get();
    if (!desired.isNone() && desired != this->nativeCaret())
    {
      this->phase_ = RESTORING;
      this->restoreSelection();
      this->phase_ = IDLE;
      this->delivery_ = scene::PaintAnswer::nativeScheduled();
    }
    else
    {
      const scene::PaintDamage empty = {paintScope(), 0, 0, 0, 0, scene::PAINT_COVERAGE_PAINT_ONLY};
      this->delivery_ = scene::PaintAnswer::exact(empty);
    }
    return;
  }
  this->replaceProjection();
}
std::string Win32TextEditorContext::committedLine(int index) const
{
  std::size_t start = 0;
  for (int i = 0; i < index; ++i)
  {
    const std::size_t end = this->projection_.text.find('\r', start);
    if (end == std::string::npos)
      return std::string();
    start = end + 1;
  }
  const std::size_t end = this->projection_.text.find('\r', start);
  return this->projection_.text.substr(start, end == std::string::npos ? end : end - start);
}
bool Win32TextEditorContext::readLine(int index, std::string &out) const
{
  const LRESULT offset = SendMessageW(this->hwnd_, EM_LINEINDEX, index, 0);
  if (offset < 0)
    return false;
  const LRESULT length = SendMessageW(this->hwnd_, EM_LINELENGTH, static_cast<WPARAM>(offset), 0);
  if (length < 0 || length > TextEditorProps::kMaxBytes)
    return false;
  wchar_t line[TextEditorProps::kMaxBytes + 1];
  line[0] = static_cast<wchar_t>(TextEditorProps::kMaxBytes);
  const LRESULT copied = SendMessageW(this->hwnd_, EM_GETLINE, index, reinterpret_cast<LPARAM>(line));
  return copied == length && loka::win32::TextEditorFromWide(line, static_cast<std::size_t>(copied), out);
}
LineCursor Win32TextEditorContext::nativeCaret() const
{
  if (!this->node_ || !this->node_->props.lines_)
    return LineCursor::None();
  DWORD start = 0, end = 0;
  SendMessageW(this->hwnd_, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
  const int index = static_cast<int>(SendMessageW(this->hwnd_, EM_LINEFROMCHAR, end, 0));
  if (index < 0 || index >= this->node_->props.lines_->size())
    return LineCursor::None();
  const int offset = static_cast<int>(SendMessageW(this->hwnd_, EM_LINEINDEX, index, 0));
  return LineCursor(this->node_->props.lines_->at(static_cast<unsigned short>(index)).id,
                    static_cast<int>(end) - offset);
}
EditorResult Win32TextEditorContext::applyLines(int first, int oldCount, int newCount)
{
  loka::core::ObservableList<loka::core::String> &lines = *this->node_->props.lines_;
  if (first < 0 || first >= lines.size() || first + oldCount > lines.size())
    return EDITOR_INVALID_CURSOR;
  std::string one;
  if (!this->readLine(first, one))
    return EDITOR_NON_ASCII;
  const loka::core::ItemId id = lines.at(static_cast<unsigned short>(first)).id;
  if (oldCount == 1 && newCount == 1)
  {
    const std::string old = this->committedLine(first);
    const LineCursor after = this->nativeCaret();
    if (one.size() > old.size() && after.line == id)
    {
      const std::size_t added = one.size() - old.size();
      if (after.column >= 0 && static_cast<std::size_t>(after.column) >= added)
      {
        const std::size_t column = static_cast<std::size_t>(after.column) - added;
        if (column <= old.size() && one.compare(0, column, old, 0, column) == 0
            && one.compare(column + added, std::string::npos, old, column, std::string::npos) == 0)
          return this->node_->document.applyKeystroke(
              LineCursor(id, static_cast<int>(column)), one.data() + column, added);
      }
    }
    return this->node_->document.applySingleLine(
        id, loka::core::String::Utf8(one.data(), one.size()), this->nativeCaret());
  }
  if (oldCount == 1 && newCount == 2)
  {
    std::string two;
    if (!this->readLine(first + 1, two) || one + two != this->committedLine(first))
      return EDITOR_INVALID_CURSOR;
    return this->node_->document.applySplit(id, static_cast<LineCursor::Column>(one.size()));
  }
  if (oldCount == 2 && newCount == 1 && one == this->committedLine(first) + this->committedLine(first + 1))
    return this->node_->document.applyJoin(lines.at(static_cast<unsigned short>(first + 1)).id);
  return EDITOR_INVALID_CURSOR;
}
EditorResult Win32TextEditorContext::commitNativeChange(bool haveSelection)
{
  if (!this->node_ || !this->node_->props.lines_)
    return EDITOR_UNAVAILABLE;
  if (!this->projection_.current(*this->node_))
    return EDITOR_STALE_ID;
  const int oldCount = this->node_->props.lines_->size();
  const int newCount = static_cast<int>(SendMessageW(this->hwnd_, EM_GETLINECOUNT, 0, 0));
  if (newCount > TextEditorProps::kMaxLines
      || GetWindowTextLengthW(this->hwnd_) > TextEditorProps::kMaxBytes + newCount - 1)
    return EDITOR_CAPACITY;
  // Native before/after selection identifies ordinary one-line edits, Enter,
  // and Backspace at line start without reading the full native document.
  const LineCursor after = this->nativeCaret();
  const int afterLine = static_cast<int>(SendMessageW(this->hwnd_, EM_LINEFROMCHAR, static_cast<WPARAM>(-1), 0));
  if (haveSelection && this->selection_.firstLine == this->selection_.lastLine)
  {
    const int first = this->selection_.firstLine;
    if (newCount == oldCount && afterLine == first && !after.isNone())
      return this->applyLines(first, 1, 1);
    if (newCount == oldCount + 1 && afterLine == first + 1)
      return this->applyLines(first, 1, 2);
    if (newCount == oldCount - 1 && first > 0 && afterLine == first - 1)
      return this->applyLines(first - 1, 2, 1);
  }
  // Undo and selection replacement may not retain a useful native range.
  // Compare bounded logical lines against the last committed projection.
  int prefix = 0;
  std::string line;
  while (prefix < oldCount && prefix < newCount && this->readLine(prefix, line) && line == this->committedLine(prefix))
    ++prefix;
  int suffix = 0;
  while (suffix < oldCount - prefix && suffix < newCount - prefix && this->readLine(newCount - suffix - 1, line)
         && line == this->committedLine(oldCount - suffix - 1))
    ++suffix;
  if (prefix == oldCount && prefix == newCount)
    return this->node_->document.moveCaret(after);
  // An unchanged prefix can include the first half of a split/join (for
  // example joining an empty following line). Include its retained identity.
  if (prefix > 0 && (oldCount == prefix + suffix || newCount == prefix + suffix))
    --prefix;
  return this->applyLines(prefix, oldCount - prefix - suffix, newCount - prefix - suffix);
}
bool Win32TextEditorContext::handleCommand(WPARAM wParam, LPARAM)
{
  if (HIWORD(wParam) != EN_CHANGE)
    return false;
  if (this->phase_ == RESTORING)
    return true;
  if (this->phase_ != INPUT && this->phase_ != IDLE)
  {
    if (this->phase_ != RETRY)
      this->phase_ = REJECTED;
    return true;
  }
  const bool outsideInput = this->phase_ == IDLE;
  if (outsideInput)
    this->captureSelection();
  this->phase_ = COMMIT;
  const HWND window = this->hwnd_;
  const EditorResult result = this->status_ == EDITOR_OK ? this->commitNativeChange(!outsideInput) : this->status_;
  if (fromWindow(window) != this)
    return true;
  if (result == EDITOR_OK && this->node_)
  {
    this->status_ = this->projection_.capture(*this->node_);
    this->delivery_ = scene::PaintAnswer::nativeScheduled();
  }
  if (result != EDITOR_OK || this->status_ != EDITOR_OK || this->phase_ == REJECTED)
  {
    if (result != EDITOR_OK)
      this->status_ = result;
    this->phase_ = REJECTED;
  }
  else
    this->phase_ = INPUT;
  if (outsideInput)
  {
    if (this->phase_ == REJECTED)
      this->deferRestore();
    else
      this->phase_ = IDLE;
  }
  return true;
}
void Win32TextEditorContext::syncCaret()
{
  if (!this->node_ || this->phase_ != INPUT || this->status_ != EDITOR_OK)
    return;
  const LineCursor cursor = this->nativeCaret();
  if (cursor == this->node_->props.cursor_.state()->get())
    return;
  this->phase_ = COMMIT;
  const HWND window = this->hwnd_;
  const EditorResult result = this->node_->document.moveCaret(cursor);
  if (fromWindow(window) != this)
    return;
  this->phase_ = result == EDITOR_OK && this->phase_ != REJECTED ? INPUT : REJECTED;
}
LRESULT CALLBACK Win32TextEditorContext::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  Win32TextEditorContext *self = fromWindow(window);
  if (!self)
    return DefWindowProcW(window, message, wParam, lParam);
  if (message == WM_TIMER && wParam == kRestoreTimer)
  {
    KillTimer(window, kRestoreTimer);
    if (self->phase_ == RETRY)
      self->restoreCommittedProjection();
    return 0;
  }
  if (!isInputMessage(message))
    return CallWindowProcW(self->previousProc_, window, message, wParam, lParam);
  if (self->phase_ != IDLE)
  {
    if (self->phase_ == INPUT || self->phase_ == COMMIT)
      self->phase_ = REJECTED;
    return 0;
  }
  if (!self->node_ || self->node_->lifecycleFact() != scene::NODE_FACT_ATTACHED || self->status_ != EDITOR_OK)
    return 0;
  self->captureSelection();
  self->phase_ = INPUT;
  const LRESULT result = CallWindowProcW(self->previousProc_, window, message, wParam, lParam);
  if (fromWindow(window) != self)
    return result;
  if (!self->hwnd_ || !self->node_)
    return result;
  self->syncCaret();
  if (fromWindow(window) != self)
    return result;
  const bool rejected = self->phase_ == REJECTED;
  self->phase_ = IDLE;
  if (rejected)
    self->restoreCommittedProjection();
  return result;
}
short Win32TextEditorContext::layout(scene::IPlatformController *, scene::LayoutState &state)
{
  this->relayout(state);
  return static_cast<short>(state.y + state.height + state.spacing);
}
void Win32TextEditorContext::relayout(const scene::LayoutState &state)
{
  if (this->hwnd_)
    this->positionNativeWindow(this->hwnd_,
                               this->controller()->displayScale().projectFrame(
                                   loka::core::Frame(state.x, state.y, state.width, state.height)));
}
scene::PaintAnswer Win32TextEditorContext::queryPaintDamage(const scene::PaintQuery &query) const
{
  if (!this->hwnd_)
    return scene::PaintAnswer::refused(scene::PAINT_REFUSED_NO_CONTEXT);
  if (query.placement != scene::PLACEMENT_ELIGIBLE)
    return scene::PaintAnswer::refused(scene::PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (this->phase_ == INPUT || this->phase_ == COMMIT)
  {
    // The native edit already owns its repaint; synchronous model echoes can
    // precede the committed-cache refresh, just as in EditText::applyText.
    const scene::PaintDamage empty = {query.scope, 0, 0, 0, 0, scene::PAINT_COVERAGE_PAINT_ONLY};
    return scene::PaintAnswer::exact(empty);
  }
  if (this->phase_ == RETRY || (this->status_ == EDITOR_OK && this->node_ && !this->projection_.current(*this->node_)))
    return scene::PaintAnswer::refused(scene::PAINT_REFUSED_PROPS_UNRECONCILED);
  scene::PaintAnswer answer = this->delivery_;
  if (answer.kind == scene::PAINT_ANSWER_EXACT)
    answer.damage.scope = query.scope;
  return answer;
}
void RegisterWin32TextEditorNodeHandler(scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&editorHandler);
}
