#include "MacTextEditorContext.hpp"
#include "../MacScenePlatformController.hpp"
#include "../MacObjCCompat.hpp"
#include "../platform/MacNativeGeometry.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "platform/StringUTF8.hpp"
#include <algorithm>
#include <new>

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

/** Informal NSTextDelegate/NSTextView delegate selectors also work on ObjC1 SDKs. */
@interface LokaTextEditorDelegate : NSObject
{
  MacTextEditorContext *owner_;
}
@property(nonatomic, assign) MacTextEditorContext *owner;
- (void)restoreProjection;
@end
@implementation LokaTextEditorDelegate
@synthesize owner = owner_;
- (void)textDidChange:(NSNotification *)notification
{
  (void)notification;
  if ([self owner])
    [self owner]->handleTextDidChange();
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
  NSTextView *view = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 200, 80)];
  LokaTextEditorDelegate *delegate = [[LokaTextEditorDelegate alloc] init];
  if (!this->projection_ || !scroll || !view || !delegate)
  {
    [delegate release];
    [view release];
    [scroll release];
    return;
  }
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
    this->projection_->phase = Projection::IDLE;
    this->syncFromNode(true);
  }
  else
  {
    [NSObject cancelPreviousPerformRequestsWithTarget:delegate];
    [delegate setOwner:0];
    [view setDelegate:nil];
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
    this->syncFromNode(false);
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
  [view setFrameSize:NSMakeSize(size.width, std::max(size.height, [view frame].size.height))];
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
  this->syncFromNode(true);
}

void MacTextEditorContext::syncFromNode(bool force)
{
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return;
  Projection &p = *this->projection_;
  NSScrollView *scroll = (NSScrollView *)this->scroll_;
  NSTextView *view = (NSTextView *)[scroll documentView];
  const NSRect visible = [scroll documentVisibleRect];
  p.phase = Projection::APPLYING;
  const EditorResult result = this->node_->document.project(p.scratch);
  if (result != EDITOR_OK)
  {
    ReplaceEditorString(view, @"", *this->controller());
    [view setEditable:NO];
    [[view undoManager] removeAllActions];
    p.clear();
    p.phase = Projection::UNAVAILABLE;
    if (result == EDITOR_ALLOCATION)
      this->scheduleRestore();
    return;
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
    return;
  }
  const bool replace = force || ![[view string] isEqualToString:desired];
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
    return;
  }
  if (!same)
  {
    [p.committed release];
    p.committed = desired;
  }
  const ObservableList<String> &lines = *this->node_->props.lines_;
  for (unsigned short i = 0; i < TextEditorProps::kMaxLines; ++i)
    p.ids[i] = i < lines.size() ? lines.at(i).id : ItemId::none();
  NativeLines native(p.committed);
  const LineCursor cursor = this->node_->props.cursor_.state()->get();
  int index = lines.find(cursor.line);
  NSRange selection = p.selection;
  if (index >= 0)
  {
    const NSUInteger location =
        native.ranges[index].location
        + std::min(native.ranges[index].length, static_cast<NSUInteger>(std::max(0, cursor.column)));
    if (replace || selection.location != location)
      selection = NSMakeRange(location, 0);
  }
  selection.location = std::min(selection.location, [p.committed length]);
  selection.length = std::min(selection.length, [p.committed length] - selection.location);
  [view setSelectedRange:selection];
  p.selection = selection;
  p.style(view, *this->node_, *this->controller(), replace);
  [[scroll contentView] scrollToPoint:visible.origin];
  [scroll reflectScrolledClipView:[scroll contentView]];
  [view setEditable:YES];
  p.phase = Projection::IDLE;
}

void MacTextEditorContext::captureSelection()
{
  if (this->projection_->phase == Projection::IDLE)
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
  if (p.phase != Projection::IDLE || !this->node_)
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
  p.phase = Projection::INPUT;
  const EditorResult result = this->node_->document.moveCaret(cursor);
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return;
  if (result != EDITOR_OK || p.phase == Projection::RECONCILE)
    this->scheduleRestore();
  else
  {
    p.selection = selection;
    p.phase = Projection::IDLE;
  }
}

EditorResult MacTextEditorContext::applyNativeChange()
{
  Projection &p = *this->projection_;
  NSTextView *view = (NSTextView *)[(NSScrollView *)this->scroll_ documentView];
  NSString *text = [view string];
  const EditorResult available = this->node_->document.project(p.scratch);
  if (available != EDITOR_OK)
    return available;
  if ([p.committed length] != p.scratch.size())
    return EDITOR_STALE_ID;
  for (NSUInteger i = 0; i < p.scratch.size(); ++i)
    if ([p.committed characterAtIndex:i] != (p.scratch[i] == '\r' ? '\n' : p.scratch[i]))
      return EDITOR_STALE_ID;
  const ObservableList<String> &committedLines = *this->node_->props.lines_;
  for (unsigned short i = 0; i < committedLines.size(); ++i)
    if (p.ids[i] != committedLines.at(i).id)
      return EDITOR_STALE_ID;
  const NativeLines before(p.committed), after(text);
  if (after.result != EDITOR_OK)
    return after.result;
  if (after.count > before.count + 1 || before.count > after.count + 1)
    return EDITOR_INVALID_CURSOR;
  unsigned short first = 0;
  while (first < before.count && first < after.count
         && EqualLine(p.committed, before.ranges[first], text, after.ranges[first]))
    ++first;
  unsigned short oldEnd = before.count, newEnd = after.count;
  while (oldEnd > first && newEnd > first
         && EqualLine(p.committed, before.ranges[oldEnd - 1], text, after.ranges[newEnd - 1]))
  {
    --oldEnd;
    --newEnd;
  }
  const NSRange selection = [view selectedRange];
  const unsigned short caretLine = after.lineAt(selection.location);
  if (first == before.count && first == after.count)
    return this->node_->document.moveCaret(
        LineCursor(p.ids[caretLine], static_cast<int>(selection.location - after.ranges[caretLine].location)));
  // A split at an endpoint (or its inverse) can leave an empty diff span
  // on one side. Include an adjacent unchanged line as the structural source.
  // The concatenation checks below still require a pure split/join.
  if (before.count != after.count && (oldEnd == first || newEnd == first))
  {
    if (first)
      --first;
    else
    {
      ++oldEnd;
      ++newEnd;
    }
  }
  if (after.count == before.count + 1)
  {
    if (oldEnd != first + 1 || newEnd != first + 2)
      return EDITOR_INVALID_CURSOR;
    const NSRange source = before.ranges[first];
    const NSRange left = after.ranges[first], right = after.ranges[first + 1];
    if (left.length + right.length != source.length
        || !EqualLine(p.committed, NSMakeRange(source.location, left.length), text, left)
        || !EqualLine(p.committed, NSMakeRange(source.location + left.length, right.length), text, right))
      return EDITOR_INVALID_CURSOR;
    return this->node_->document.applySplit(p.ids[first], static_cast<int>(left.length));
  }
  if (before.count == after.count + 1)
  {
    if (oldEnd != first + 2 || newEnd != first + 1)
      return EDITOR_INVALID_CURSOR;
    const NSRange left = before.ranges[first], right = before.ranges[first + 1], joined = after.ranges[first];
    if (joined.length != left.length + right.length
        || !EqualLine(p.committed, left, text, NSMakeRange(joined.location, left.length))
        || !EqualLine(p.committed, right, text, NSMakeRange(joined.location + left.length, right.length)))
      return EDITOR_INVALID_CURSOR;
    return this->node_->document.applyJoin(p.ids[first + 1]);
  }
  if (oldEnd != first + 1 || newEnd != first + 1)
    return EDITOR_INVALID_CURSOR;
  const NSRange oldLine = before.ranges[first], newLine = after.ranges[first];
  // Infer insertion position from text too: undo need not use the saved
  // selection. Use the keystroke door only when its resulting caret matches.
  NSUInteger column = 0;
  while (column < oldLine.length && column < newLine.length &&
         [p.committed characterAtIndex:oldLine.location + column] == [text characterAtIndex:newLine.location + column])
    ++column;
  if (newLine.length > oldLine.length)
  {
    const NSUInteger added = newLine.length - oldLine.length;
    if (caretLine == first && selection.location == newLine.location + column + added
        && EqualLine(p.committed,
                     NSMakeRange(oldLine.location + column, oldLine.length - column),
                     text,
                     NSMakeRange(newLine.location + column + added, oldLine.length - column)))
    {
      NSString *insert = [text substringWithRange:NSMakeRange(newLine.location + column, added)];
      return this->node_->document.applyKeystroke(
          LineCursor(p.ids[first], static_cast<int>(column)), [insert UTF8String], added);
    }
  }
  NSString *replacement = [text substringWithRange:newLine];
  return this->node_->document.applySingleLine(
      p.ids[first],
      String::Utf8([replacement UTF8String], newLine.length),
      LineCursor(p.ids[caretLine], static_cast<int>(selection.location - after.ranges[caretLine].location)));
}

void MacTextEditorContext::handleTextDidChange()
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
  const EditorResult result = this->applyNativeChange();
  if (!this->node_ || this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED)
    return;
  if (result != EDITOR_OK || p.phase == Projection::RECONCILE)
    this->scheduleRestore();
  else
    this->syncFromNode(false);
}

void RegisterMacTextEditorNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&handler);
}
