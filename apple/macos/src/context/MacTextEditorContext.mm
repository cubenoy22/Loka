#include "MacTextEditorContext.hpp"
#include "app/nodes/controls/TextChangeSpan.hpp"
#include "app/nodes/controls/TextEditorDiff.hpp"
#include "../MacScenePlatformController.hpp"
#include "../MacObjCCompat.hpp"
#include "../platform/MacNativeGeometry.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "platform/StringUTF8.hpp"
#include <algorithm>
#include <new>
#include <cstdio>

using namespace loka::app;
using namespace loka::core;

namespace
{
  /** Logical lines, including a final empty line. Native wrapping is irrelevant. */
  struct NativeLines
  {
    NSRange ranges[TextEditorProps::kMaxLines];
    unsigned short count;
    EditorResult result;
    explicit NativeLines(NSString *text)
        : count(0),
          result(EDITOR_OK)
    {
      const NSUInteger length = [text length];
      if (length > TextEditorProps::kMaxBytes)
      {
        this->result = EDITOR_CAPACITY;
        return;
      }
      for (NSUInteger i = 0; i < length; ++i)
      {
        const unichar c = [text characterAtIndex:i];
        if (!c || c > 127)
        {
          this->result = EDITOR_NON_ASCII;
          return;
        }
      }
      NSUInteger offset = 0;
      do
      {
        if (this->count == TextEditorProps::kMaxLines)
        {
          this->result = EDITOR_CAPACITY;
          return;
        }
        if (offset == length)
        {
          this->ranges[this->count++] = NSMakeRange(offset, 0);
          break;
        }
        NSUInteger start = offset, end = offset, contentsEnd = offset;
        [text getLineStart:&start end:&end contentsEnd:&contentsEnd forRange:NSMakeRange(offset, 0)];
        this->ranges[this->count++] = NSMakeRange(offset, contentsEnd - offset);
        if (end == length && contentsEnd == length)
          break;
        offset = end;
      } while (offset <= length);
    }
    unsigned short lineAt(NSUInteger offset) const
    {
      unsigned short i = 0;
      while (i + 1 < this->count && offset >= this->ranges[i + 1].location)
        ++i;
      return i;
    }
  };

  bool EqualLine(NSString *a, NSRange ar, NSString *b, NSRange br)
  {
    if (ar.length != br.length)
      return false;
    for (NSUInteger i = 0; i < ar.length; ++i)
      if ([a characterAtIndex:ar.location + i] != [b characterAtIndex:br.location + i])
        return false;
    return true;
  }

  /** Borrowed indexed characters for the rail-neutral column diff. */
  class NativeCharacters
  {
  public:
    NativeCharacters(NSString *text, NSUInteger offset)
        : text_(text),
          offset_(offset)
    {
    }
    unichar operator[](std::size_t index) const
    {
      return [this->text_ characterAtIndex:this->offset_ + index];
    }
  private:
    NSString *text_;
    NSUInteger offset_;
  };

  void InstallEditorFont(NSTextView *view, MacScenePlatformController &controller)
  {
    NSFont *font = (NSFont *)controller.textFont(TextStyle(), true);
    if (!font)
      return;
    [view setFont:font];
    NSTextStorage *storage = [view textStorage];
    [storage beginEditing];
    [storage addAttribute:NSFontAttributeName value:font range:NSMakeRange(0, [storage length])];
    [storage endEditing];
    [view setTypingAttributes:[NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName]];
  }

  void ReplaceEditorString(NSTextView *view, NSString *text, MacScenePlatformController &controller)
  {
    [view setString:text];
    // Full replacement resets attributes, including those on line separators.
    InstallEditorFont(view, controller);
  }

  class MacTextEditorHandler
      : public loka::app::scene::RetainedNodeHandler<MacTextEditorHandler,
                                                     loka::app::TextEditorNode,
                                                     MacTextEditorContext>
  {
  public:
    static loka::app::TextEditorNode *cast(loka::app::scene::Node *node)
    {
      return node && node->nodeTypeKey() == loka::app::scene::NodeTypeToken<loka::app::TextEditorNode>()
                 ? static_cast<loka::app::TextEditorNode *>(node)
                 : 0;
    }
    static MacTextEditorContext *create(loka::app::TextEditorNode *node,
                                        loka::app::scene::IPlatformController *controller,
                                        const loka::app::scene::LayoutState &)
    {
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      MacTextEditorContext *context = new (std::nothrow) MacTextEditorContext(mac, mac->projectionParentView(), node);
      if (context && !context->hasNativeView())
      {
        delete context;
        return 0;
      }
      return context;
    }
  };
  MacTextEditorHandler handler;
} // namespace

/** Informal text view/storage delegate selectors also work on ObjC1 SDKs. */
@interface LokaTextEditorDelegate : NSObject
{
  MacTextEditorContext *owner_;
}
@property(nonatomic, assign) MacTextEditorContext *owner;
- (void)restoreProjection;
- (void)applyHighlights;
@end
@implementation LokaTextEditorDelegate
@synthesize owner = owner_;
- (void)textDidChange:(NSNotification *)notification
{
  NSTextView *view = (NSTextView *)[notification object];
  if ([self owner])
    [self owner]->handleTextDidChange(MacTextEditorContext::VIEW_CHANGE, [view selectedRange].location);
}
- (void)textStorageDidProcessEditing:(NSNotification *)notification
{
  NSTextStorage *storage = (NSTextStorage *)[notification object];
  if (([storage editedMask] & NSTextStorageEditedCharacters) && [self owner])
  {
    // Highlighting is deferred: this context applies no attributes inside
    // processEditing, so our styling no longer widens this character-edit range.
    // It still unions all character edits in the pass: evidence of where,
    // not of how much changed. Selection is not final here.
    [self owner]->handleTextDidChange(MacTextEditorContext::STORAGE_EDIT, [storage editedRange].location);
  }
}
- (void)textViewDidChangeSelection:(NSNotification *)notification
{
  (void)notification;
  if ([self owner])
    [self owner]->handleSelectionDidChange();
}
- (BOOL)textView:(NSTextView *)view shouldChangeTextInRange:(NSRange)range replacementString:(NSString *)text
{
  (void)view;
  (void)range;
  (void)text;
  if ([self owner])
    [self owner]->captureSelection();
  return YES;
}
- (void)applyHighlights
{
  if ([self owner])
    [self owner]->applyHighlights();
}
- (void)restoreProjection
{
  if ([self owner])
    [self owner]->restoreCommittedProjection();
}
@end

/** All tentative state shares one phase. The retained payload exists before
    native input; refusing an unchanged document needs no new NSString. */
struct MacTextEditorContext::Projection
{
  enum Phase
  {
    IDLE,
    INPUT,
    RECONCILE,
    STORAGE_PENDING,
    QUEUED,
    APPLYING,
    UNAVAILABLE
  };
  struct Styles
  {
    ItemId id;
    LineHighlight highlight;
    Styles *next;
    Styles(ItemId key, const String &text, const LineHighlighter *policy)
        : id(key),
          highlight(text, policy),
          next(0)
    {
    }
  };
  Phase phase;
  NSString *committed;
  std::string scratch;
  ItemId ids[TextEditorProps::kMaxLines];
  NSRange selection;
  Styles *styles;
  Projection()
      : phase(UNAVAILABLE),
        committed(nil),
        selection(NSMakeRange(0, 0)),
        styles(0)
  {
    this->scratch.reserve(TextEditorProps::kMaxBytes);
  }
  ~Projection()
  {
    this->clear();
  }
  void clearStyles()
  {
    while (this->styles)
    {
      Styles *row = this->styles;
      this->styles = row->next;
      delete row;
    }
  }
  void clear()
  {
    [this->committed release];
    this->committed = nil;
    this->clearStyles();
    for (unsigned short i = 0; i < TextEditorProps::kMaxLines; ++i)
      this->ids[i] = ItemId::none();
  }
  /** Validate the snapshot before consuming native text or deferred styling. */
  EditorResult validateDocument(TextEditorNode &node)
  {
    const EditorResult available = node.document.project(this->scratch);
    if (available != EDITOR_OK)
      return available;
    if ([this->committed length] != this->scratch.size())
      return EDITOR_STALE_ID;
    for (NSUInteger i = 0; i < this->scratch.size(); ++i)
      if ([this->committed characterAtIndex:i] != (this->scratch[i] == '\r' ? '\n' : this->scratch[i]))
        return EDITOR_STALE_ID;
    const ObservableList<String> &committedLines = *node.props.lines_;
    for (unsigned short i = 0; i < committedLines.size(); ++i)
      if (this->ids[i] != committedLines.at(i).id)
        return EDITOR_STALE_ID;
    return EDITOR_OK;
  }
  void style(NSTextView *view, loka::app::TextEditorNode &node, MacScenePlatformController &controller, bool force)
  {
    NativeLines lines([view string]);
    if (lines.result != EDITOR_OK)
      return;
    Styles *old = this->styles;
    this->styles = 0;
    Styles **tail = &this->styles;
    NSTextStorage *storage = [view textStorage];
    [storage beginEditing];
    for (unsigned short i = 0; i < node.props.lines_->size(); ++i)
    {
      const ObservableList<String>::Entry &entry = node.props.lines_->at(i);
      Styles **slot = &old;
      while (*slot && (*slot)->id != entry.id)
        slot = &(*slot)->next;
      Styles *row = *slot;
      bool changed = true;
      if (row)
      {
        *slot = row->next;
        const LineHighlight next(entry.value, node.props.highlighter_, &row->highlight);
        changed = next.value() != row->highlight.value();
        row->highlight = next;
      }
      else
        row = new (std::nothrow) Styles(entry.id, entry.value, node.props.highlighter_);
      if (force || changed || !row)
      {
        NSFont *font = (NSFont *)controller.textFont(TextStyle(), true);
        NSDictionary *defaults = font ? [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName] : nil;
        [storage setAttributes:defaults range:lines.ranges[i]];
        if (row && row->highlight.value().valid())
        {
          const AttributedString &value = row->highlight.value();
          std::string joined;
          bool valid = true;
          for (std::size_t run = 0; run < value.segmentCount(); ++run)
          {
            std::string bytes;
            if (!loka::platform::CollectUtf8(value.segment(run).text, bytes))
              valid = false;
            joined += bytes;
          }
          NSString *styled = [[[NSString alloc] initWithBytes:joined.data()
                                                       length:joined.size()
                                                     encoding:NSASCIIStringEncoding] autorelease];
          if (valid && styled && EqualLine(styled, NSMakeRange(0, [styled length]), [view string], lines.ranges[i]))
          {
            NSUInteger offset = lines.ranges[i].location;
            for (std::size_t run = 0; run < value.segmentCount(); ++run)
            {
              std::string bytes;
              NSFont *runFont = (NSFont *)controller.textFont(value.segment(run).style, true);
              if (!runFont || !loka::platform::CollectUtf8(value.segment(run).text, bytes))
              {
                valid = false;
                break;
              }
              [storage addAttribute:NSFontAttributeName value:runFont range:NSMakeRange(offset, bytes.size())];
              offset += bytes.size();
            }
            if (!valid)
              [storage setAttributes:defaults range:lines.ranges[i]];
          }
        }
      }
      if (row)
      {
        row->next = 0;
        *tail = row;
        tail = &row->next;
      }
    }
    [storage endEditing];
    while (old)
    {
      Styles *next = old->next;
      delete old;
      old = next;
    }
  }
};

MacTextEditorContext::MacTextEditorContext(MacScenePlatformController *controller,
                                         void *parent,
                                         loka::app::TextEditorNode *node)
    : MacRetirableContext(controller),
      projection_(new(std::nothrow) Projection()),
      restores_(0),
      node_(node),
      parent_(parent),
      scroll_(0),
      delegate_(0)
{
  NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  NSTextView *view = [[NSTextView alloc] initWithFrame:NSZeroRect];
  LokaTextEditorDelegate *delegate = [[LokaTextEditorDelegate alloc] init];
  if (!this->projection_ || !scroll || !view || !delegate)
  {
    [delegate release];
    [view release];
    [scroll release];
    return;
  }
  loka::macos::SetMacFrame(view, controller->projection().projectFrame(loka::core::Frame(0, 0, 200, 80)));
  [view setRichText:NO];
  [view setAllowsUndo:YES];
  InstallEditorFont(view, *controller);
  [view setVerticallyResizable:YES];
  [view setMaxSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];
  [scroll setHasVerticalScroller:YES];
  [scroll setDocumentView:view];
  [view release];
  [delegate setOwner:this];
  this->scroll_ = scroll;
  this->delegate_ = delegate;
}

MacTextEditorContext::~MacTextEditorContext()
{
  assert(!this->scroll_ && !this->delegate_ && "detach must queue native objects before reclaim");
  delete this->projection_;
}
bool MacTextEditorContext::hasNativeView() const
{
  return this->scroll_ != 0;
}

void MacTextEditorContext::readLifecycleFactOnAttach()
{
  this->onFactChanged(loka::app::scene::NODE_FACT_DETACHED_RETAINED, this->node_->lifecycleFact());
}
void MacTextEditorContext::onFactChanged(loka::app::scene::NodeLifecycleFact, loka::app::scene::NodeLifecycleFact next)
{
  NSScrollView *scroll = (NSScrollView *)this->scroll_;
  NSTextView *view = (NSTextView *)[scroll documentView];
  LokaTextEditorDelegate *delegate = (LokaTextEditorDelegate *)this->delegate_;
  if (next == loka::app::scene::NODE_FACT_ATTACHED)
  {
    [(NSView *)this->parent_ addSubview:scroll];
    [delegate setOwner:this];
    [view setDelegate:(id)delegate];
    [[view textStorage] setDelegate:(id)delegate];
    this->projection_->phase = Projection::IDLE;
    const bool retry = this->syncFromNode(true);
    this->consumePendingRequest();
    if (retry)
      this->scheduleRestore();
  }
  else
  {
    [NSObject cancelPreviousPerformRequestsWithTarget:delegate];
    [delegate setOwner:0];
    [view setDelegate:nil];
    [[view textStorage] setDelegate:nil];
    [view setEditable:NO];
    [scroll removeFromSuperview];
    this->projection_->phase = Projection::UNAVAILABLE;
    this->projection_->clear();
    if (next == loka::app::scene::NODE_FACT_RETIRED)
    {
      this->retireNativeObjects(this->scroll_, this->delegate_);
      this->node_ = 0;
      this->parent_ = 0;
    }
  }
}
void MacTextEditorContext::onPropsApplied()
{
  if (this->projection_->phase == Projection::IDLE || this->projection_->phase == Projection::UNAVAILABLE)
  {
    const bool retry = this->syncFromNode(false);
    this->consumePendingRequest();
    if (retry)
      this->scheduleRestore();
  }
}
short MacTextEditorContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  if (state.height <= 0)
    state.height = 80;
  NSScrollView *scroll = (NSScrollView *)this->scroll_;
  loka::macos::SetMacFrame(
      scroll,
      this->controller()->projection().projectFrame(loka::core::Frame(state.x, state.y, state.width, state.height)));
  NSTextView *view = (NSTextView *)[scroll documentView];
  const NSSize size = [scroll contentSize];
  [view setMinSize:size];
  const loka::macos::MacProjection &projection = this->controller()->projection();
  const int documentHeight = projection.measurementToLu(std::max(size.height, [view frame].size.height));
  loka::macos::SetMacDocumentHeight(scroll, projection.projectLength(0, documentHeight));
  this->onPropsApplied();
  return static_cast<short>(state.y + state.height + state.spacing);
}

void MacTextEditorContext::scheduleRestore()
{
  Projection &p = *this->projection_;
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED || p.phase == Projection::QUEUED)
    return;
  p.phase = Projection::QUEUED;
  [(NSTextView *)[(NSScrollView *)this->scroll_ documentView] setEditable:NO];
  [(LokaTextEditorDelegate *)this->delegate_ performSelector:@selector(restoreProjection) withObject:nil afterDelay:0];
}
void MacTextEditorContext::restoreCommittedProjection()
{
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return;
  ++this->restores_;
  const bool retry = this->syncFromNode(true);
  this->consumePendingRequest();
  if (retry)
    this->scheduleRestore();
}

bool MacTextEditorContext::syncFromNode(bool force, bool nativeCommit)
{
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return false;
  Projection &p = *this->projection_;
  NSScrollView *scroll = (NSScrollView *)this->scroll_;
  NSTextView *view = (NSTextView *)[scroll documentView];
  const NSRect visible = [scroll documentVisibleRect];
  // A repair inside the consumer must not reopen request delivery.
  const Projection::Phase completion = p.phase == Projection::RECONCILE ? Projection::RECONCILE : Projection::IDLE;
  p.phase = Projection::APPLYING;
  const EditorResult result = this->node_->document.project(p.scratch);
  if (result != EDITOR_OK)
  {
    if (nativeCommit)
    {
      this->scheduleRestore();
      return false;
    }
    ReplaceEditorString(view, @"", *this->controller());
    [view setEditable:NO];
    [[view undoManager] removeAllActions];
    p.clear();
    p.phase = Projection::UNAVAILABLE;
    // The entry refuses pending requests before scheduling an allocation retry.
    return result == EDITOR_ALLOCATION;
  }
  // NSString uses LF; the owner seam serializes CR. ASCII keeps offsets equal.
  std::replace(p.scratch.begin(), p.scratch.end(), '\r', '\n');
  NSString *desired = p.committed;
  bool same = desired && [desired length] == p.scratch.size();
  for (NSUInteger i = 0; same && i < p.scratch.size(); ++i)
    same = [desired characterAtIndex:i] == static_cast<unsigned char>(p.scratch[i]);
  if (!same)
    desired = [[NSString alloc] initWithBytes:p.scratch.data() length:p.scratch.size() encoding:NSASCIIStringEncoding];
  if (!desired)
  {
    p.clear();
    this->scheduleRestore();
    return false;
  }
  const bool replace = force || ![[view string] isEqualToString:desired];
  if (replace && nativeCommit)
  {
    if (!same)
      [desired release];
    this->scheduleRestore();
    return false;
  }
  if (replace)
  {
    ReplaceEditorString(view, desired, *this->controller());
    [[view undoManager] removeAllActions];
  }
  if ([[view string] length] != p.scratch.size() || ![[view string] isEqualToString:desired])
  {
    if (!same)
      [desired release];
    p.clearStyles();
    this->scheduleRestore();
    return false;
  }
  if (!same)
  {
    [p.committed release];
    p.committed = desired;
  }
  const ObservableList<String> &lines = *this->node_->props.lines_;
  for (unsigned short i = 0; i < TextEditorProps::kMaxLines; ++i)
    p.ids[i] = i < lines.size() ? lines.at(i).id : ItemId::none();
  if (nativeCommit)
  {
    // Accept the snapshot without writing selection or attributes during input.
    p.phase = Projection::IDLE;
    return false;
  }
  if (replace)
  {
    // Restoring/replacing text also restores its committed selection. A fact
    // notification over unchanged text must never move the native caret.
    NativeLines native(p.committed);
    const LineCursor cursor = this->node_->props.cursorState()->get();
    const int index = lines.find(cursor.line);
    NSRange selection = p.selection;
    if (index >= 0)
    {
      const NSUInteger location =
          native.ranges[index].location
          + std::min(native.ranges[index].length, static_cast<NSUInteger>(std::max(0, cursor.column)));
      selection = NSMakeRange(location, 0);
    }
    selection.location = std::min(selection.location, [p.committed length]);
    selection.length = std::min(selection.length, [p.committed length] - selection.location);
    [view setSelectedRange:selection];
    p.selection = selection;
  }
  p.style(view, *this->node_, *this->controller(), replace);
  const loka::macos::MacProjection &projection = this->controller()->projection();
  loka::macos::ScrollMacDocument(view, projection.scrollOffsetToNative(projection.scrollPositionToLu(visible.origin.y)));
  [scroll reflectScrolledClipView:[scroll contentView]];
  [view setEditable:YES];
  p.phase = completion;
  return false;
}

void MacTextEditorContext::consumePendingRequest()
{
  // Platform twin of Null/Toolbox: one take and one epilogue take. Further
  // reposts stay dirty for the next props apply; nested delivery stays closed.
  if (this->consumeRequest())
    this->consumeRequest();
}
bool MacTextEditorContext::consumeRequest()
{
  Projection &p = *this->projection_;
  if (!this->node_ || (p.phase != Projection::IDLE && p.phase != Projection::UNAVAILABLE))
    return false;
  const loka::app::scene::WriteSeat<LineCursor> request = this->node_->props.moveCaretTo_.request_;
  if (!request.isValid() || request.state()->get().isNone())
    return false;
  const Projection::Phase completion = p.phase;
  p.phase = Projection::INPUT;
  const LineCursor pending = request.state()->get();
  const TextEditorProps binding = this->node_->props;
  request.set(LineCursor::None());
  if (completion == Projection::IDLE && this->node_ && p.phase == Projection::INPUT && this->scroll_
      && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED && this->node_->props.lines_ == binding.lines_
      && this->node_->props.moveCaretTo_.state() == request.state()
      && this->node_->document.availability() == EDITOR_OK)
  {
    StateTracker *owner = 0;
    if (binding.lines_->queryMutationTracker(owner) == EDIT_OK && request.usesTracker(owner)
        && binding.lines_->find(pending.line) >= 0)
    {
      NSTextView *view = (NSTextView *)[(NSScrollView *)this->scroll_ documentView];
      // Taking may notify an owner edit. Repair before interpreting offsets.
      if (p.validateDocument(*this->node_) != EDITOR_OK || ![[view string] isEqualToString:p.committed])
      {
        p.phase = Projection::RECONCILE;
        if (this->syncFromNode(false))
          this->scheduleRestore();
        if (p.phase == Projection::RECONCILE)
          p.phase = Projection::INPUT;
      }
      if (this->node_ && p.phase == Projection::INPUT
          && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED
          && this->node_->props.lines_ == binding.lines_
          && this->node_->props.moveCaretTo_.state() == request.state())
      {
        const NativeLines native(p.committed);
        const int index = binding.lines_->find(pending.line);
        if (native.result == EDITOR_OK && index >= 0 && index < native.count)
        {
          const NSRange row = native.ranges[index];
          const NSUInteger column = std::min(row.length, static_cast<NSUInteger>(std::max(0, pending.column)));
          const LineCursor clamped(pending.line, static_cast<int>(column));
          p.phase = Projection::APPLYING;
          [view setSelectedRange:NSMakeRange(row.location + column, 0)];
          if (this->node_ && p.phase == Projection::APPLYING
              && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
          {
            p.selection = [view selectedRange];
            p.phase = Projection::INPUT;
            if (this->node_->props.lines_ == binding.lines_
                && this->node_->props.moveCaretTo_.state() == request.state())
              this->node_->document.moveCaret(clamped);
          }
        }
      }
    }
  }
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return true;
  if (completion == Projection::IDLE && (p.phase == Projection::INPUT || p.phase == Projection::RECONCILE))
  {
    NSTextView *view = (NSTextView *)[(NSScrollView *)this->scroll_ documentView];
    if (p.phase == Projection::RECONCILE || p.validateDocument(*this->node_) != EDITOR_OK
        || ![[view string] isEqualToString:p.committed])
    {
      p.phase = Projection::RECONCILE;
      if (this->syncFromNode(false))
        this->scheduleRestore();
    }
  }
  if (p.phase == Projection::INPUT || p.phase == Projection::RECONCILE)
    p.phase = completion;
  return true;
}

void MacTextEditorContext::captureSelection()
{
  if (this->projection_->phase == Projection::IDLE || this->projection_->phase == Projection::STORAGE_PENDING)
    this->projection_->selection = [(NSTextView *)[(NSScrollView *)this->scroll_ documentView] selectedRange];
}

void MacTextEditorContext::handleSelectionDidChange()
{
  Projection &p = *this->projection_;
  if (p.phase == Projection::INPUT || p.phase == Projection::RECONCILE)
  {
    p.phase = Projection::RECONCILE;
    return;
  }
  if ((p.phase != Projection::IDLE && p.phase != Projection::STORAGE_PENDING) || !this->node_)
    return;
  NSTextView *view = (NSTextView *)[(NSScrollView *)this->scroll_ documentView];
  // NSTextView can announce selection before textDidChange. Only a selection
  // over committed text is a caret-only event.
  if (![[view string] isEqualToString:p.committed])
    return;
  const NativeLines lines(p.committed);
  const NSRange selection = [view selectedRange];
  const unsigned short index = lines.lineAt(selection.location);
  const LineCursor cursor(p.ids[index], static_cast<int>(selection.location - lines.ranges[index].location));
  const Projection::Phase completion = p.phase;
  p.phase = Projection::INPUT;
  const EditorResult result = this->node_->document.moveCaret(cursor);
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return;
  if (result != EDITOR_OK || p.phase == Projection::RECONCILE)
    this->scheduleRestore();
  else
  {
    p.selection = selection;
    p.phase = completion;
  }
  this->consumePendingRequest();
}

EditorResult MacTextEditorContext::applyNativeChange(TextObservation source, std::size_t &caretOffset)
{
  Projection &p = *this->projection_;
  NSTextView *view = (NSTextView *)[(NSScrollView *)this->scroll_ documentView];
  NSString *text = [view string];
  const EditorResult available = p.validateDocument(*this->node_);
  if (available != EDITOR_OK)
    return available;
  const NativeLines before(p.committed), after(text);
  if (after.result != EDITOR_OK)
    return after.result;
  // validateDocument left the committed CR-separated snapshot in scratch.
  // Normalize through native line ranges so CRLF is also one logical separator.
  std::string logical;
  logical.reserve([text length]);
  for (unsigned short i = 0; i < after.count; ++i)
  {
    if (i)
      logical += '\r';
    const NSRange range = after.ranges[i];
    for (NSUInteger column = 0; column < range.length; ++column)
      logical += static_cast<char>([text characterAtIndex:range.location + column]);
  }
  int hintLine = -1, hintColumn = -1;
  if (source == STORAGE_EDIT)
  {
    // Interpret mutation evidence in the before snapshot, as for view input.
    // Missing evidence leaves the hint absent; append-only passes clamp to EOF.
    if (before.result == EDITOR_OK && before.count && caretOffset != NSNotFound)
    {
      const NSUInteger location = std::min(static_cast<NSUInteger>(caretOffset), [p.committed length]);
      hintLine = before.lineAt(location);
      const NSRange range = before.ranges[hintLine];
      hintColumn = static_cast<int>(std::min(location - range.location, range.length));
    }
  }
  else if (!p.selection.length && p.selection.location <= [p.committed length])
  {
    // Like Win32, interpret the captured native selection in the before snapshot.
    hintLine = before.lineAt(p.selection.location);
    hintColumn = static_cast<int>(p.selection.location - before.ranges[hintLine].location);
  }
  const TextEditorLineDiff diff = DiffTextEditorLines(p.scratch, logical, hintLine, hintColumn);
  const unsigned short first = static_cast<unsigned short>(diff.first());
  if (!diff.before() && !diff.after())
  {
    if (source == STORAGE_EDIT)
      return EDITOR_OK;
    const NSUInteger caret = std::min(static_cast<NSUInteger>(caretOffset), [text length]);
    const unsigned short caretLine = after.lineAt(caret);
    // A later view notification can carry a new caret over committed text.
    const LineCursor cursor(p.ids[caretLine], static_cast<int>(caret - after.ranges[caretLine].location));
    return cursor == this->node_->props.cursorState()->get() ? EDITOR_OK : this->node_->document.moveCaret(cursor);
  }
  const unsigned short oldEnd = static_cast<unsigned short>(first + diff.before());
  const unsigned short newEnd = static_cast<unsigned short>(first + diff.after());
  const NSRange oldRange = NSMakeRange(before.ranges[first].location,
                                     NSMaxRange(before.ranges[oldEnd - 1]) - before.ranges[first].location);
  const NSRange newRange = NSMakeRange(after.ranges[first].location,
                                     NSMaxRange(after.ranges[newEnd - 1]) - after.ranges[first].location);
  if (source == STORAGE_EDIT)
  {
    // Storage announces text before the view's selection is final. Compare the
    // complete changed span, including separators, to find the insertion end.
    const loka::app::detail::TextChangeSpan span(
        NativeCharacters(p.committed, oldRange.location), oldRange.length,
        NativeCharacters(text, newRange.location), newRange.length);
    caretOffset = newRange.location + span.afterEnd();
  }
  const NSUInteger caret = std::min(static_cast<NSUInteger>(caretOffset), [text length]);
  const unsigned short caretLine = after.lineAt(caret);
  std::string replacement;
  for (unsigned short i = first; i < newEnd; ++i)
  {
    if (i != first)
      replacement += '\r';
    NSString *line = [text substringWithRange:after.ranges[i]];
    replacement.append([line UTF8String], [line lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
  }
  return this->node_->document.applyReplace(
      LineCursor(p.ids[first], 0),
      LineCursor(p.ids[oldEnd - 1], static_cast<int>(before.ranges[oldEnd - 1].length)),
      replacement.data(), replacement.size(),
      RowCursor(caretLine, static_cast<int>(caret - after.ranges[caretLine].location)));
}

void MacTextEditorContext::handleTextDidChange(TextObservation source, std::size_t caretOffset)
{
  Projection &p = *this->projection_;
  if (p.phase == Projection::APPLYING || p.phase == Projection::UNAVAILABLE)
    return;
  if (p.phase == Projection::INPUT || p.phase == Projection::RECONCILE)
  {
    p.phase = Projection::RECONCILE;
    return;
  }
  if (p.phase == Projection::QUEUED)
    return;
  p.phase = Projection::INPUT;
#ifndef NDEBUG
  NSTextView *view = (NSTextView *)[(NSScrollView *)this->scroll_ documentView];
  const bool textChanged = ![[view string] isEqualToString:p.committed];
#endif
  const EditorResult result = this->applyNativeChange(source, caretOffset);
#ifndef NDEBUG
  // Scalar-only diagnostics identify the committing callback without user text.
  if (result != EDITOR_OK || p.phase == Projection::RECONCILE)
    fprintf(stderr, "[MacTextEditor %s] text-diff=%d caret-offset=%lu result=%d phase=%d\n",
            source == STORAGE_EDIT ? "textStorageDidProcessEditing:" : "textDidChange:",
            static_cast<int>(textChanged), static_cast<unsigned long>(caretOffset),
            static_cast<int>(result), static_cast<int>(p.phase));
#endif
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return;
  if (result != EDITOR_OK || p.phase == Projection::RECONCILE)
    this->scheduleRestore();
  else
  {
    this->syncFromNode(false, true);
    if (p.phase != Projection::IDLE)
      return;
    LokaTextEditorDelegate *delegate = (LokaTextEditorDelegate *)this->delegate_;
    [NSObject cancelPreviousPerformRequestsWithTarget:delegate selector:@selector(applyHighlights) object:nil];
    if (source == VIEW_CHANGE)
    {
      p.selection = [(NSTextView *)[(NSScrollView *)this->scroll_ documentView] selectedRange];
      this->projectHighlights();
      this->consumePendingRequest();
      if (p.phase == Projection::UNAVAILABLE)
        this->scheduleRestore();
    }
    else
    {
      p.phase = Projection::STORAGE_PENDING;
      [delegate performSelector:@selector(applyHighlights) withObject:nil afterDelay:0];
    }
  }
}

void MacTextEditorContext::applyHighlights()
{
  this->projectHighlights();
  this->consumePendingRequest();
  if (this->projection_->phase == Projection::UNAVAILABLE)
    this->scheduleRestore();
}

void MacTextEditorContext::projectHighlights()
{
  Projection &p = *this->projection_;
  if ((p.phase != Projection::IDLE && p.phase != Projection::STORAGE_PENDING) || !this->node_
      || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return;
  NSTextView *view = (NSTextView *)[(NSScrollView *)this->scroll_ documentView];
  if (![[view string] isEqualToString:p.committed] || p.validateDocument(*this->node_) != EDITOR_OK)
  {
    p.phase = Projection::UNAVAILABLE;
    return;
  }
  p.phase = Projection::APPLYING;
  p.style(view, *this->node_, *this->controller(), false);
  p.phase = Projection::IDLE;
}

void RegisterMacTextEditorNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&handler);
}
