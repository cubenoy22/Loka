#include "MacTextEditorTests.hpp"
#include "context/MacTextEditorContext.hpp"
#include "MacScenePlatformController.hpp"
#include "MacObjCCompat.hpp"
#include "platform/MacNativeGeometry.hpp"
#include "support/TestVerify.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/LokaAllocFailure.hpp"
#include "platform/StringUTF8.hpp"
#include <vector>
#include <algorithm>

namespace loka
{
  namespace testing
  {
    class MacTextEditorAccess
    {
    public:
      static unsigned restores(const MacTextEditorContext &context)
      {
        return context.restores_;
      }
    };
  } // namespace testing
} // namespace loka

/** A native setter which refuses once; the production context must detect it. */
@interface LokaRefusingEditorView : NSTextView
{
  BOOL refuse_;
}
@property(nonatomic, assign) BOOL refuse;
@end
@implementation LokaRefusingEditorView
@synthesize refuse = refuse_;
- (void)setString:(NSString *)string
{
  if ([self refuse])
    [self setRefuse:NO];
  else
    [super setString:string];
}
@end

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  typedef loka::testing::MacTextEditorAccess Access;

  struct NativeHost
  {
    NSAutoreleasePool *pool;
    NSWindow *window;
    NativeHost()
        : pool([[NSAutoreleasePool alloc] init]),
          window(nil)
    {
      [NSApplication sharedApplication];
      this->window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 320, 200)
                                                 styleMask:LOKA_MAC_WINDOW_STYLE_TITLED
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
      [this->window setReleasedWhenClosed:NO];
    }
    ~NativeHost()
    {
      [this->window close];
      [this->window release];
      [this->pool drain];
    }
  };
  struct Fixture
  {
    NativeHost host;
    PushStateTracker tracker;
    ObservableList<String> lines;
    MutableState<LineCursor> cursor;
    NodeState<LineCursor> seat;
    MacScenePlatformController controller;
    TextEditorNode node;
    MacTextEditorContext *context;
    NSScrollView *scroll;
    NSTextView *view;
    explicit Fixture(unsigned short count = 3, const std::string &text = "abcd")
        : host(),
          tracker(),
          lines(),
          cursor(),
          seat(&cursor, &tracker),
          controller([host.window contentView], RailMetrics()),
          node(TextEditorProps(lines, seat)),
          context(0),
          scroll(nil),
          view(nil)
    {
      this->tracker.addState(&this->cursor);
      LOKA_VERIFY(this->lines.attach(&this->tracker, 258) == ATTACH_OK);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(this->lines.insert(i, String(text)) == EDIT_OK);
      if (count)
      {
        StateTrackerGuard guard(&this->tracker);
        this->cursor.set(LineCursor(this->lines.at(0).id, std::min(2, static_cast<int>(text.size()))));
      }
      LayoutState state;
      state.x = 3;
      state.y = 5;
      state.width = 240;
      state.height = 96;
      state.spacing = 4;
      LOKA_VERIFY(this->controller.prepareProjectedLayout(&this->node, state));
      this->context = static_cast<MacTextEditorContext *>(this->node.getContext());
      LOKA_VERIFY(this->context);
      this->scroll = (NSScrollView *)[[[this->host.window contentView] subviews] objectAtIndex:0];
      this->view = (NSTextView *)[this->scroll documentView];
      LOKA_VERIFY(![this->view isRichText]);
      LOKA_VERIFY([this->view font] == (NSFont *)this->controller.textFont(TextStyle(), true));
      LOKA_VERIFY(this->context->layout(&this->controller, state) == 105);
      LOKA_VERIFY(state.height == 96);
      LOKA_VERIFY(
          NSEqualRects([this->scroll frame], this->controller.projection().projectFrame(Frame(3, 5, 240, 96)).r));
      [this->host.window makeFirstResponder:this->view];
    }
    ~Fixture()
    {
      LifecycleFactTestAccess::MarkSubtreeRetired(&this->node);
      LifecycleFactTestAccess::DeliverFacts(&this->node);
    }
    void notify()
    {
      [[this->view delegate] textDidChange:[NSNotification notificationWithName:NSTextDidChangeNotification
                                                                         object:this->view]];
    }
    void edit(NSString *text, NSUInteger caret)
    {
      this->context->captureSelection();
      [this->view setString:text];
      [this->view setSelectedRange:NSMakeRange(caret, 0)];
      this->notify();
    }
    void turn()
    {
      [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    void restored(unsigned count)
    {
      // This assertion also proves restore did not run inside textDidChange.
      LOKA_VERIFY(Access::restores(*this->context) == count - 1);
      for (int i = 0; i < 20 && Access::restores(*this->context) < count; ++i)
        this->turn();
      LOKA_VERIFY(Access::restores(*this->context) == count);
      std::string bytes;
      LOKA_VERIFY(this->node.document.project(bytes) == EDITOR_OK);
      std::replace(bytes.begin(), bytes.end(), '\r', '\n');
      NSString *expected = [[[NSString alloc] initWithBytes:bytes.data()
                                                     length:bytes.size()
                                                   encoding:NSASCIIStringEncoding] autorelease];
      LOKA_VERIFY([[this->view string] isEqualToString:expected]);
      LOKA_VERIFY([[this->view string] length] == bytes.size());
      LOKA_VERIFY([this->view isEditable]);
    }
  };
  struct Snapshot
  {
    std::vector<ItemId> ids;
    std::vector<String> text;
    ListRevision revision;
    LineCursor cursor;
    explicit Snapshot(const Fixture &f)
        : revision(f.lines.revision().get()),
          cursor(f.cursor.get())
    {
      for (unsigned short i = 0; i < f.lines.size(); ++i)
      {
        this->ids.push_back(f.lines.at(i).id);
        this->text.push_back(f.lines.at(i).value);
      }
    }
    void unchanged(const Fixture &f) const
    {
      LOKA_VERIFY(this->ids.size() == f.lines.size());
      for (unsigned short i = 0; i < f.lines.size(); ++i)
      {
        LOKA_VERIFY(this->ids[i] == f.lines.at(i).id);
        LOKA_VERIFY(this->text[i].equals(f.lines.at(i).value));
      }
      LOKA_VERIFY(!(this->revision != f.lines.revision().get()));
      LOKA_VERIFY(this->cursor == f.cursor.get());
    }
  };
  struct Observer
  {
    Fixture &fixture;
    unsigned calls;
    bool nested;
    bool deferred;
    Observer(Fixture &f)
        : fixture(f),
          calls(0),
          nested(false),
          deferred(false)
    {
      const_cast<State<ListRevision> &>(f.lines.revision()).bind(&changed, this, false);
    }
    ~Observer()
    {
      const_cast<State<ListRevision> &>(this->fixture.lines.revision()).unbind(&changed, this);
    }
    static void reenter(void *data)
    {
      Observer &self = *static_cast<Observer *>(data);
      self.fixture.edit(@"abxZcd\nabcd\nabcd", 4);
    }
    static void changed(void *data)
    {
      Observer &self = *static_cast<Observer *>(data);
      ++self.calls;
      if (self.nested)
      {
        self.nested = false;
        if (self.deferred)
          self.fixture.tracker.defer(&reenter, &self);
        else
          reenter(&self);
      }
    }
  };
  struct CaretObserver
  {
    Fixture &fixture;
    bool nested;
    unsigned calls;
    explicit CaretObserver(Fixture &f)
        : fixture(f),
          nested(true),
          calls(0)
    {
      f.cursor.bind(&changed, this, false);
    }
    ~CaretObserver()
    {
      this->fixture.cursor.unbind(&changed, this);
    }
    static void changed(void *data)
    {
      CaretObserver &self = *static_cast<CaretObserver *>(data);
      ++self.calls;
      if (self.nested)
      {
        self.nested = false;
        [self.fixture.view setSelectedRange:NSMakeRange(8, 0)];
        [[self.fixture.view delegate]
            textViewDidChangeSelection:[NSNotification notificationWithName:NSTextViewDidChangeSelectionNotification
                                                                     object:self.fixture.view]];
      }
    }
  };
  std::string bytes(const String &value)
  {
    std::string out;
    LOKA_VERIFY(loka::platform::CollectUtf8(value, out));
    return out;
  }
  void nextKey(Fixture &f)
  {
    const NSRange selection = [f.view selectedRange];
    NSMutableString *text = [[[f.view string] mutableCopy] autorelease];
    [text replaceCharactersInRange:selection withString:@"y"];
    const ListRevision before = f.lines.revision().get();
    f.edit(text, selection.location + 1);
    LOKA_VERIFY(f.lines.revision().get().content == before.content + 1);
    LOKA_VERIFY([f.view isEditable]);
  }
} // namespace

void testMacTextEditorLineActions()
{
  Fixture f;
  Observer observer(f);
  const ItemId first = f.lines.at(0).id, second = f.lines.at(1).id;
  const String unchanged = f.lines.at(2).value;
  f.edit(@"abxcd\nabcd\nabcd", 3);
  LOKA_VERIFY(observer.calls == 1);
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_UPDATE);
  LOKA_VERIFY(f.lines.at(0).id == first && f.lines.at(1).id == second);
  LOKA_VERIFY(f.lines.at(2).value.compare(unchanged, false) == StringCompareEqual);
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd" && f.cursor.get() == LineCursor(first, 3));
  f.edit(@"abx\ncd\nabcd\nabcd", 4);
  LOKA_VERIFY(observer.calls == 2 && f.lines.size() == 4);
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
  LOKA_VERIFY(f.lines.at(2).id == second && f.cursor.get() == LineCursor(f.lines.at(1).id, 0));
  f.edit(@"abxcd\nabcd\nabcd", 3);
  LOKA_VERIFY(observer.calls == 3 && f.lines.size() == 3);
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
  LOKA_VERIFY(f.lines.at(1).id == second && f.cursor.get() == LineCursor(first, 3));
  [f.view setSelectedRange:NSMakeRange(7, 0)];
  [[f.view delegate]
      textViewDidChangeSelection:[NSNotification notificationWithName:NSTextViewDidChangeSelectionNotification
                                                               object:f.view]];
  LOKA_VERIFY(observer.calls == 3 && f.cursor.get() == LineCursor(second, 1));
  // Native selection replacement and deletion remain a single line UPDATE.
  [f.view setSelectedRange:NSMakeRange(7, 2)];
  [[f.view delegate]
      textViewDidChangeSelection:[NSNotification notificationWithName:NSTextViewDidChangeSelectionNotification
                                                               object:f.view]];
  f.context->onPropsApplied();
  LOKA_VERIFY(NSEqualRanges([f.view selectedRange], NSMakeRange(7, 2)));
  f.edit(@"abxcd\naQd\nabcd", 8);
  LOKA_VERIFY(observer.calls == 4 && bytes(f.lines.at(1).value) == "aQd");
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_UPDATE);
  f.edit(@"abxcd\nad\nabcd", 7);
  LOKA_VERIFY(observer.calls == 5 && bytes(f.lines.at(1).value) == "ad");
  LOKA_VERIFY(Access::restores(*f.context) == 0);
}

void testMacTextEditorRefusals()
{
  for (int failure = 0; failure < 5; ++failure)
  {
    Fixture f(failure == 0 ? 256 : 3);
    Observer observer(f);
    if (failure == 1)
    {
      LOKA_VERIFY(f.lines.detach() == EDIT_OK);
      LOKA_VERIFY(f.lines.attach(&f.tracker, 258) == ATTACH_OK);
      LOKA_VERIFY(f.lines.insert(0, String("fresh")) == EDIT_OK);
      observer.calls = 0;
    }
    const Snapshot snapshot(f);
    const NSRange selection = [f.view selectedRange];
    const NSRect visible = [f.scroll documentVisibleRect];
    // Positive undo control, before proving that cancellation removes it.
    NSUndoManager *undo = [f.view undoManager];
    LOKA_VERIFY(undo != nil);
    [undo beginUndoGrouping];
    [[undo prepareWithInvocationTarget:f.view] setString:@"refused undo payload"];
    [undo endUndoGrouping];
    LOKA_VERIFY([undo canUndo]);
    NSMutableString *tentative = [[[f.view string] mutableCopy] autorelease];
    [tentative insertString:(failure == 0 || failure == 2 ? @"\n" : @"x") atIndex:selection.location];
    if (failure == 2)
      loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 1);
    if (failure == 3)
      [tentative appendString:[@"x" stringByPaddingToLength:8193 withString:@"x" startingAtIndex:0]];
    if (failure == 4)
      [tentative appendString:@"\u00e9"];
    f.edit(tentative, selection.location + 1);
    loka::core::testing::allowLokaAllocRaw();
    snapshot.unchanged(f);
    LOKA_VERIFY(observer.calls == 0);
    LOKA_VERIFY(![f.view isEditable]);
    f.restored(1);
    snapshot.unchanged(f);
    LOKA_VERIFY(observer.calls == 0 && ![undo canUndo]);
    LOKA_VERIFY(NSEqualRanges([f.view selectedRange], selection));
    LOKA_VERIFY(NSEqualPoints([f.scroll documentVisibleRect].origin, visible.origin));
    nextKey(f);
  }
}

void testMacTextEditorNestedInput()
{
  for (int deferred = 0; deferred < 2; ++deferred)
  {
    Fixture f;
    Observer observer(f);
    observer.nested = true;
    observer.deferred = deferred != 0;
    f.edit(@"abxcd\nabcd\nabcd", 3);
    LOKA_VERIFY(observer.calls == 1 && bytes(f.lines.at(0).value) == "abxcd");
    LOKA_VERIFY(f.cursor.get() == LineCursor(f.lines.at(0).id, 3));
    f.restored(1);
    LOKA_VERIFY(observer.calls == 1);
    nextKey(f);
  }
  {
    Fixture f;
    Observer listObserver(f);
    CaretObserver observer(f);
    [f.view setSelectedRange:NSMakeRange(1, 0)];
    [[f.view delegate]
        textViewDidChangeSelection:[NSNotification notificationWithName:NSTextViewDidChangeSelectionNotification
                                                                 object:f.view]];
    LOKA_VERIFY(observer.calls == 1 && listObserver.calls == 0);
    LOKA_VERIFY(f.cursor.get() == LineCursor(f.lines.at(0).id, 1));
    f.restored(1);
    LOKA_VERIFY(NSEqualRanges([f.view selectedRange], NSMakeRange(1, 0)));
    nextKey(f);
  }
}

void testMacTextEditorReplacementFailure()
{
  Fixture f;
  LokaRefusingEditorView *view = [[LokaRefusingEditorView alloc] initWithFrame:[f.view frame]];
  [view setString:[f.view string]];
  [view setSelectedRange:[f.view selectedRange]];
  [view setDelegate:[f.view delegate]];
  [f.view setDelegate:nil];
  [f.scroll setDocumentView:view];
  f.view = view;
  [view release];
  f.edit(@"\u00e9", 1);
  [view setRefuse:YES];
  // Invoke the scheduled callback after the notification has unwound, without
  // spinning the run loop into its retry. The retry must remain deferred.
  id delegate = [view delegate];
  [NSObject cancelPreviousPerformRequestsWithTarget:delegate];
  [delegate performSelector:@selector(restoreProjection)];
  LOKA_VERIFY(Access::restores(*f.context) == 1);
  LOKA_VERIFY(![view isEditable] && [[view string] isEqualToString:@"\u00e9"]);
  f.restored(2);
  nextKey(f);
}

namespace
{
  class Highlighter : public LineHighlighter
  {
  public:
    mutable unsigned calls;
    bool fail;
    Highlighter()
        : calls(0),
          fail(false)
    {
    }
    virtual bool highlight(const String &line, AttributedString::Builder &out) const
    {
      ++this->calls;
      return !this->fail && out.append(line, Bold + Italic + FontSize<18>());
    }
  };
} // namespace
void testMacTextEditorHighlightAndLifecycle()
{
  Highlighter highlighter;
  Fixture f;
  f.node.props.highlighter(highlighter);
  f.context->onPropsApplied();
  LOKA_VERIFY(highlighter.calls == 3);
  f.context->onPropsApplied();
  LOKA_VERIFY(highlighter.calls == 3);
  f.edit(@"abxcd\nabcd\nabcd", 3);
  LOKA_VERIFY(highlighter.calls == 4);
  NSFont *styled = [[f.view textStorage] attribute:NSFontAttributeName atIndex:0 effectiveRange:0];
  LOKA_VERIFY(styled == (NSFont *)f.controller.textFont(Bold + Italic + FontSize<18>(), true));
  highlighter.fail = true;
  f.edit(@"abxycd\nabcd\nabcd", 4);
  LOKA_VERIFY(highlighter.calls == 5 && bytes(f.lines.at(0).value) == "abxycd");
  LOKA_VERIFY([[f.view textStorage] attribute:NSFontAttributeName atIndex:0 effectiveRange:0]
              == (NSFont *)f.controller.textFont(TextStyle(), true));
  highlighter.fail = false;
  loka::core::testing::failLokaAllocRaw("AttributedString", "Segments", 1);
  f.edit(@"abxyQcd\nabcd\nabcd", 5);
  loka::core::testing::allowLokaAllocRaw();
  LOKA_VERIFY(highlighter.calls == 6 && bytes(f.lines.at(0).value) == "abxyQcd");
  LOKA_VERIFY([[f.view textStorage] attribute:NSFontAttributeName atIndex:0 effectiveRange:0]
              == (NSFont *)f.controller.textFont(TextStyle(), true));
  // Pending restore is invalidated by retained detach, as is the style cache.
  f.edit(@"\u00e9", 1);
  NotifySubtreeNodeDetached(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  LOKA_VERIFY([f.scroll superview] == nil && [f.view delegate] == nil);
  f.turn();
  LOKA_VERIFY(Access::restores(*f.context) == 0);
  NotifySubtreeNodeAttached(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  LOKA_VERIFY([f.scroll superview] == [f.host.window contentView] && [f.view delegate] != nil);
  LOKA_VERIFY([[f.view string] isEqualToString:@"abxyQcd\nabcd\nabcd"]);
  nextKey(f);
  // An external over-cap value makes the editor unavailable, never truncated.
  LOKA_VERIFY(f.lines.update(f.lines.at(0).id, String(std::string(8193, 'a'))) == EDIT_OK);
  f.context->onPropsApplied();
  LOKA_VERIFY(![f.view isEditable] && [[f.view string] length] == 0);
  LOKA_VERIFY(f.lines.update(f.lines.at(0).id, String("recovered")) == EDIT_OK);
  f.context->onPropsApplied();
  LOKA_VERIFY([f.view isEditable]);
  LifecycleFactTestAccess::MarkSubtreeRetired(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  LOKA_VERIFY([f.scroll superview] == nil && [f.view delegate] == nil);
}
