#include "support/TextEditorStateOwner.hpp"
#include "support/TextEditorAccess.hpp"
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
#include <cstdio>

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

/** Counts the exact native door used by the rail, including equal selections. */
@interface LokaRequestEditorView : NSTextView
{
  NSUInteger selectionWrites_;
  NSArray *selectionWriteStack_;
}
@property(nonatomic, assign) NSUInteger selectionWrites;
@property(nonatomic, retain) NSArray *selectionWriteStack;
@end
@implementation LokaRequestEditorView
@synthesize selectionWrites = selectionWrites_;
@synthesize selectionWriteStack = selectionWriteStack_;
- (void)setSelectedRange:(NSRange)range
{
  // Count only writes the rail or the test itself issued. AppKit's own text
  // system also calls this setter to fix the selection after a storage edit
  // (NSTextLayoutManager _fixSelectionAfterChangeInCharacterRange, measured on
  // hosted CI for #878); that write is not the rail's and must not be pinned.
  NSArray *stack = [NSThread callStackSymbols];
  NSString *caller = [stack count] > 1 ? [stack objectAtIndex:1] : @"";
  const BOOL fromTextSystem =
      [caller rangeOfString:@"UIFoundation"].location != NSNotFound || [caller rangeOfString:@"AppKit"].location != NSNotFound;
  if (!fromTextSystem)
  {
    [self setSelectionWrites:[self selectionWrites] + 1];
    [self setSelectionWriteStack:stack];
  }
  [super setSelectedRange:range];
}
- (void)dealloc
{
  [self setSelectionWriteStack:nil];
  [super dealloc];
}
@end

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  typedef loka::testing::MacTextEditorAccess Access;

  void verifyFont(NSFont *actual, NSFont *expected)
  {
    LOKA_VERIFY(actual != nil && expected != nil);
    LOKA_VERIFY([[actual fontName] isEqualToString:[expected fontName]]);
    LOKA_VERIFY([actual pointSize] == [expected pointSize]);
  }

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
  struct Fixture : loka::app::testing::TextEditorStateOwner
  {
    NativeHost host;
    ObservableList<String> lines;
    MacScenePlatformController controller;
    TextEditorNode node;
    MacTextEditorContext *context;
    NSScrollView *scroll;
    NSTextView *view;
    explicit Fixture(unsigned short count = 3, const std::string &text = "abcd")
        : host(),
          lines(),
          controller([host.window contentView], RailMetrics()),
          node(TextEditorProps(lines, cursor).moveCaretTo(request)),
          context(0),
          scroll(nil),
          view(nil)
    {
      LOKA_VERIFY(this->lines.attach(&this->tracker, 258) == ATTACH_OK);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(this->lines.insert(i, String(text)) == EDIT_OK);
      // The initial caret is a fact the seam publishes, not an app request:
      // this rail delivers requests only from PR 4 of #873 onward.
      // Over-capacity fixtures keep the seam's refusal; every other fixture seeds.
      if (count)
      {
        const EditorResult seeded = loka::app::testing::TextEditorAccess::document(this->node).moveCaret(
            LineCursor(this->lines.at(0).id, std::min(2, static_cast<int>(text.size()))));
        LOKA_VERIFY(seeded == EDITOR_OK || seeded == EDITOR_CAPACITY);
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
      this->verifyPlainFont();
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
    void verifyPlainFont()
    {
      NSFont *font = (NSFont *)this->controller.textFont(TextStyle(), true);
      verifyFont([this->view font], font);
      verifyFont([[this->view typingAttributes] objectForKey:NSFontAttributeName], font);
      NSTextStorage *storage = [this->view textStorage];
      for (NSUInteger i = 0; i < [storage length]; ++i)
        verifyFont([storage attribute:NSFontAttributeName atIndex:i effectiveRange:0], font);
    }
    void notify()
    {
      [[this->view delegate] textDidChange:[NSNotification notificationWithName:NSTextDidChangeNotification
                                                                         object:this->view]];
    }
    void edit(NSString *text, NSUInteger caret)
    {
      this->context->captureSelection();
      // Model a local native edit rather than a whole-document replacement.
      NSString *before = [this->view string];
      NSUInteger first = 0, oldEnd = [before length], newEnd = [text length];
      while (first < oldEnd && first < newEnd && [before characterAtIndex:first] == [text characterAtIndex:first])
        ++first;
      while (oldEnd > first && newEnd > first
             && [before characterAtIndex:oldEnd - 1] == [text characterAtIndex:newEnd - 1])
      {
        --oldEnd;
        --newEnd;
      }
      [[this->view textStorage] replaceCharactersInRange:NSMakeRange(first, oldEnd - first)
                                            withString:[text substringWithRange:NSMakeRange(first, newEnd - first)]];
      // The view publishes its final selection after storage processing.
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
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(this->node).project(bytes) == EDITOR_OK);
      std::replace(bytes.begin(), bytes.end(), '\r', '\n');
      NSString *expected = [[[NSString alloc] initWithBytes:bytes.data()
                                                     length:bytes.size()
                                                   encoding:NSASCIIStringEncoding] autorelease];
      LOKA_VERIFY([[this->view string] isEqualToString:expected]);
      LOKA_VERIFY([[this->view string] length] == bytes.size());
      LOKA_VERIFY([this->view isEditable]);
      this->verifyPlainFont();
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
          cursor(f.cursor.state()->get())
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
      LOKA_VERIFY(this->cursor == f.cursor.state()->get());
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
      f.cursor.state()->bind(&changed, this, false);
    }
    ~CaretObserver()
    {
      this->fixture.cursor.state()->unbind(&changed, this);
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
  void printEditState(Fixture &f, const char *stage)
  {
    const NSRange selection = [f.view selectedRange];
    const LineCursor cursor = f.cursor.state()->get();
    fprintf(stderr,
            "[MacTextEditor %s] model-line0=\"%s\" selectedRange=(%lu,%lu) "
            "cursor=(id=%u:%u,line-index=%d,column=%d)\n",
            stage, bytes(f.lines.at(0).value).c_str(),
            static_cast<unsigned long>(selection.location), static_cast<unsigned long>(selection.length),
            static_cast<unsigned>(cursor.line.generation), static_cast<unsigned>(cursor.line.seq),
            f.lines.find(cursor.line), cursor.column);
  }
  void printUndoState(Fixture &f, NSUndoManager *undo, const char *stage)
  {
    printEditState(f, stage);
    NSString *line = [[[f.view string] componentsSeparatedByString:@"\n"] objectAtIndex:0];
    fprintf(stderr,
            "[MacTextEditor undo %s] groupingLevel=%ld canUndo=%d native-line0=\"%s\" "
            "model-line0=\"%s\" restores=%u status(availability)=%d editable=%d\n",
            stage,
            static_cast<long>([undo groupingLevel]),
            static_cast<int>([undo canUndo]),
            [line UTF8String],
            bytes(f.lines.at(0).value).c_str(),
            Access::restores(*f.context),
            static_cast<int>(loka::app::testing::TextEditorAccess::document(f.node).availability()),
            static_cast<int>([f.view isEditable]));
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

  void printSelectionWriteMismatch(LokaRequestEditorView *view, NSUInteger expected, const char *stage)
  {
    const NSUInteger actual = [view selectionWrites];
    if (actual == expected)
      return;
    const char *stack = [[[view selectionWriteStack] description] UTF8String];
    fprintf(stderr, "[MacTextEditor %s] selectionWrites expected=%lu actual=%lu; last setSelectedRange: stack:\n%s\n",
            stage, static_cast<unsigned long>(expected), static_cast<unsigned long>(actual),
            stack ? stack : "(no captured stack)");
  }

  LokaRequestEditorView *instrumentSelection(Fixture &f)
  {
    LokaRequestEditorView *view = [[LokaRequestEditorView alloc] initWithFrame:[f.view frame]];
    [view setString:[f.view string]];
    [view setSelectedRange:[f.view selectedRange]];
    [view setDelegate:[f.view delegate]];
    [[view textStorage] setDelegate:[[f.view textStorage] delegate]];
    [f.view setDelegate:nil];
    [[f.view textStorage] setDelegate:nil];
    [f.scroll setDocumentView:view];
    f.view = view;
    [view release];
    [f.host.window makeFirstResponder:view];
    const NSUInteger before = [view selectionWrites];
    [view setSelectedRange:[view selectedRange]];
    printSelectionWriteMismatch(view, before + 1, "setter positive control");
    LOKA_VERIFY([view selectionWrites] == before + 1); // Positive control.
    LOKA_VERIFY([[view selectionWriteStack] count] > 0);
    return view;
  }

  void requestCaret(Fixture &f, LineCursor cursor)
  {
    StateTrackerGuard guard(&f.tracker);
    f.request.set(cursor);
  }

  /** Model synchronous owner props delivery while the rail reports/takes. */
  struct RequestObserver
  {
    Fixture &fixture;
    LokaRequestEditorView *view;
    State<LineCursor> *source;
    LineCursor repost;
    explicit RequestObserver(Fixture &f, LokaRequestEditorView *native, LineCursor next, bool onTake = false)
        : fixture(f), view(native), source(onTake ? f.request.state() : f.cursor.state()), repost(next)
    {
      this->source->bind(&changed, this, false);
    }
    ~RequestObserver()
    {
      this->source->unbind(&changed, this);
    }
    static void changed(void *data)
    {
      RequestObserver &self = *static_cast<RequestObserver *>(data);
      const LineCursor next = self.repost;
      self.repost = LineCursor::None();
      if (!next.isNone())
        requestCaret(self.fixture, next);
      const NSUInteger before = [self.view selectionWrites];
      self.fixture.context->onPropsApplied();
      printSelectionWriteMismatch(self.view, before, "report/take observer props completion");
      LOKA_VERIFY([self.view selectionWrites] == before);
    }
  };

  struct RepeatingRequestObserver
  {
    Fixture &fixture;
    unsigned calls;
    explicit RepeatingRequestObserver(Fixture &f) : fixture(f), calls(0)
    {
      f.cursor.state()->bind(&changed, this, false);
    }
    ~RepeatingRequestObserver()
    {
      this->fixture.cursor.state()->unbind(&changed, this);
    }
    static void changed(void *data)
    {
      RepeatingRequestObserver &self = *static_cast<RepeatingRequestObserver *>(data);
      ++self.calls;
      // Fail promptly if an unbounded epilogue is restored by a mutation.
      LOKA_VERIFY(self.calls <= 4);
      requestCaret(self.fixture, LineCursor(self.fixture.lines.at(0).id,
                                           self.fixture.cursor.state()->get().column == 0 ? 1 : 0));
      self.fixture.context->onPropsApplied();
    }
  };

  struct RepairRequestObserver
  {
    Fixture &fixture;
    LineCursor repost;
    unsigned reports;
    explicit RepairRequestObserver(Fixture &f)
        : fixture(f), repost(f.lines.at(1).id, 3), reports(0)
    {
      f.request.state()->bind(&taken, this, false);
      f.cursor.state()->bind(&reported, this, false);
    }
    ~RepairRequestObserver()
    {
      this->fixture.request.state()->unbind(&taken, this);
      this->fixture.cursor.state()->unbind(&reported, this);
    }
    static void taken(void *data)
    {
      RepairRequestObserver &self = *static_cast<RepairRequestObserver *>(data);
      if (self.repost.isNone())
        return;
      const LineCursor next = self.repost;
      self.repost = LineCursor::None();
      LOKA_VERIFY(self.fixture.lines.update(self.fixture.lines.at(0).id, String("abcdefgh")) == EDIT_OK);
      requestCaret(self.fixture, next);
      self.fixture.context->onPropsApplied();
    }
    static void reported(void *data)
    {
      RepairRequestObserver &self = *static_cast<RepairRequestObserver *>(data);
      ++self.reports;
      LOKA_VERIFY(self.reports <= 2);
      LOKA_VERIFY(self.fixture.cursor.state()->get()
                  == LineCursor(self.fixture.lines.at(1).id, self.reports == 1 ? 1 : 3));
    }
  };
} // namespace

void testMacTextEditorRequests()
{
  // New request-delivery assertions: predicted reds, not run on the macOS rig.
  {
    Fixture f;
    // The fixture seeds the fact through the seam; props deliver app requests.
    LOKA_VERIFY(NSEqualRanges([f.view selectedRange], NSMakeRange(2, 0)));
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 2));
    LOKA_VERIFY(f.request.get().isNone());
    requestCaret(f, LineCursor(f.lines.at(1).id, 99));
    f.context->onPropsApplied();
    LOKA_VERIFY(NSEqualRanges([f.view selectedRange], NSMakeRange(9, 0)));
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 4));
    LOKA_VERIFY(f.request.get().isNone());
    requestCaret(f, LineCursor(f.lines.at(1).id, -8));
    f.context->onPropsApplied();
    LOKA_VERIFY(NSEqualRanges([f.view selectedRange], NSMakeRange(5, 0)));
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 0));
    LOKA_VERIFY(f.request.get().isNone());
  }
  {
    Fixture f;
    RepeatingRequestObserver observer(f);
    requestCaret(f, LineCursor(f.lines.at(0).id, 0));
    f.context->onPropsApplied();
    LOKA_VERIFY(observer.calls == 2);
    LOKA_VERIFY(f.request.get() == LineCursor(f.lines.at(0).id, 0));
    LOKA_VERIFY(NSEqualRanges([f.view selectedRange], NSMakeRange(1, 0)));
    f.context->onPropsApplied();
    LOKA_VERIFY(observer.calls == 4);
    LOKA_VERIFY(!f.request.get().isNone());
  }
  for (int onTake = 0; onTake < 2; ++onTake)
  {
    Fixture f;
    requestCaret(f, LineCursor::None());
    LokaRequestEditorView *view = instrumentSelection(f);
    requestCaret(f, LineCursor(f.lines.at(0).id, 1));
    RequestObserver observer(f, view, LineCursor(f.lines.at(1).id, 3), onTake != 0);
    f.context->onPropsApplied();
    LOKA_VERIFY(NSEqualRanges([view selectedRange], NSMakeRange(8, 0)));
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 3));
    LOKA_VERIFY(f.request.get().isNone());
  }
  {
    Fixture f;
    requestCaret(f, LineCursor(f.lines.at(1).id, 1));
    RepairRequestObserver observer(f);
    f.context->onPropsApplied();
    LOKA_VERIFY(observer.reports == 2);
    LOKA_VERIFY([[f.view string] isEqualToString:@"abcdefgh\nabcd\nabcd"]);
    LOKA_VERIFY(NSEqualRanges([f.view selectedRange], NSMakeRange(12, 0)));
    LOKA_VERIFY(f.request.get().isNone());
  }
  {
    Fixture f;
    requestCaret(f, LineCursor::None());
    LokaRequestEditorView *view = instrumentSelection(f);
    RequestObserver observer(f, view, LineCursor(f.lines.at(1).id, 1));
    // Selection's synchronous report spends props delivery before its tail.
    [view setSelectedRange:NSMakeRange(0, 0)];
    f.context->handleSelectionDidChange();
    LOKA_VERIFY(NSEqualRanges([view selectedRange], NSMakeRange(6, 0)));
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 1));
    LOKA_VERIFY(f.request.get().isNone());
  }
  for (int storageOnly = 0; storageOnly < 2; ++storageOnly)
  {
    Fixture f;
    requestCaret(f, LineCursor::None());
    LokaRequestEditorView *view = instrumentSelection(f);
    RequestObserver observer(f, view, LineCursor(f.lines.at(1).id, 1));
    id viewDelegate = [view delegate];
    id storageDelegate = [[view textStorage] delegate];
    if (storageOnly)
      [view setDelegate:nil];
    else
      [[view textStorage] setDelegate:nil];
    f.context->captureSelection();
    const NSUInteger before = [view selectionWrites];
    [[view textStorage] replaceCharactersInRange:NSMakeRange(2, 0) withString:@"x"];
    if (storageOnly)
    {
      printSelectionWriteMismatch(view, before, "storage replace returned");
      LOKA_VERIFY([view selectionWrites] == before);
      LOKA_VERIFY(f.request.get() == LineCursor(f.lines.at(1).id, 1));
      f.context->onPropsApplied();
      printSelectionWriteMismatch(view, before, "storage-pending props completion");
      LOKA_VERIFY([view selectionWrites] == before);
      LOKA_VERIFY(!f.request.get().isNone());
      // Selection reporting is allowed, but cannot open request delivery yet.
      f.context->handleSelectionDidChange();
      LOKA_VERIFY(!f.request.get().isNone());
      for (int i = 0; i < 20 && !f.request.get().isNone(); ++i)
        f.turn();
    }
    else
    {
      [view setSelectedRange:NSMakeRange(3, 0)];
      f.notify();
    }
    [view setDelegate:viewDelegate];
    [[view textStorage] setDelegate:storageDelegate];
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd");
    LOKA_VERIFY(NSEqualRanges([view selectedRange], NSMakeRange(7, 0)));
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 1));
    LOKA_VERIFY(f.request.get().isNone());
  }
  for (int staleModel = 0; staleModel < 2; ++staleModel)
  {
    Fixture f;
    LokaRequestEditorView *view = instrumentSelection(f);
    RequestObserver observer(f, view, LineCursor(f.lines.at(1).id, 1));
    id delegate = [view delegate];
    [view setDelegate:nil];
    [[view textStorage] replaceCharactersInRange:NSMakeRange(2, 0) withString:@"x"];
    LOKA_VERIFY(!f.request.get().isNone());
    if (staleModel)
      LOKA_VERIFY(f.lines.update(f.lines.at(0).id, String(std::string(8193, 'a'))) == EDIT_OK);
    else
    {
      [[view textStorage] setDelegate:nil];
      [view setString:@"stale native"];
      [[view textStorage] setDelegate:delegate];
    }
    const LineCursor before = f.cursor.state()->get();
    const NSUInteger writes = [view selectionWrites];
    // Invoke only the queued completion, before its separately queued repair.
    [NSObject cancelPreviousPerformRequestsWithTarget:delegate];
    [delegate performSelector:@selector(applyHighlights)];
    LOKA_VERIFY(f.request.get().isNone());
    LOKA_VERIFY(f.cursor.state()->get() == before);
    printSelectionWriteMismatch(view, writes, "stale deferred completion");
    LOKA_VERIFY([view selectionWrites] == writes);
    [view setDelegate:delegate];
  }
  {
    Fixture f;
    const LineCursor before = f.cursor.state()->get();
    LOKA_VERIFY(f.lines.update(f.lines.at(0).id, String(std::string(8193, 'a'))) == EDIT_OK);
    requestCaret(f, LineCursor(f.lines.at(1).id, 1));
    f.context->onPropsApplied();
    LOKA_VERIFY(![f.view isEditable]);
    LOKA_VERIFY(f.request.get().isNone());
    LOKA_VERIFY(f.cursor.state()->get() == before);
  }
}

void testMacTextEditorRequestReverse()
{
  // Predicted, not run: setter instrumentation discriminates report echo.
  Fixture f;
  requestCaret(f, LineCursor::None());
  LokaRequestEditorView *view = instrumentSelection(f);
  [view setSelectedRange:NSMakeRange(2, 0)];
  f.context->handleSelectionDidChange();
  RequestObserver observer(f, view, LineCursor::None());
  [view deleteBackward:nil];
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "acd");
  LOKA_VERIFY(NSEqualRanges([view selectedRange], NSMakeRange(1, 0)));
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 1));
  const NSUInteger afterDelete = [view selectionWrites];
  f.context->onPropsApplied();
  printSelectionWriteMismatch(view, afterDelete, "post-delete props completion");
  LOKA_VERIFY([view selectionWrites] == afterDelete);
  for (NSUInteger offset = 2; offset <= 5; ++offset)
  {
    [view moveRight:nil];
    LOKA_VERIFY(NSEqualRanges([view selectedRange], NSMakeRange(offset, 0)));
    const LineCursor expected = offset < 4 ? LineCursor(f.lines.at(0).id, static_cast<int>(offset))
                                          : LineCursor(f.lines.at(1).id, static_cast<int>(offset - 4));
    LOKA_VERIFY(f.cursor.state()->get() == expected);
    const NSUInteger afterArrow = [view selectionWrites];
    f.context->onPropsApplied();
    printSelectionWriteMismatch(view, afterArrow, "post-arrow props completion");
    LOKA_VERIFY([view selectionWrites] == afterArrow);
  }
  LOKA_VERIFY(f.request.get().isNone());
}

void testMacTextEditorLineActions()
{
  // Programmatic storage edits must preserve identities at the mutation,
  // even when the published cursor is parked on the other identical line.
  for (int splitB = 0; splitB < 2; ++splitB)
  {
    Fixture edge(2, "a");
    const ItemId a = edge.lines.at(0).id, b = edge.lines.at(1).id;
    [edge.view setSelectedRange:NSMakeRange(splitB ? 1 : 2, 0)];
    edge.context->handleSelectionDidChange();
    LOKA_VERIFY(edge.cursor.state()->get() == (splitB ? LineCursor(a, 1) : LineCursor(b, 0)));
    id viewDelegate = [edge.view delegate];
    [edge.view setDelegate:nil];
    [[edge.view textStorage] replaceCharactersInRange:NSMakeRange(splitB ? 2 : 1, 0) withString:@"\n"];
    [edge.view setDelegate:viewDelegate];
    LOKA_VERIFY(Access::restores(*edge.context) == 0);
    LOKA_VERIFY(edge.lines.size() == 3);
    LOKA_VERIFY(edge.lines.at(0).id == a && bytes(edge.lines.at(0).value) == "a");
    LOKA_VERIFY(bytes(edge.lines.at(1).value).empty());
    LOKA_VERIFY(bytes(edge.lines.at(2).value) == "a");
    LOKA_VERIFY(edge.lines.at(splitB ? 1 : 2).id == b);
    const ItemId created = edge.lines.at(splitB ? 2 : 1).id;
    LOKA_VERIFY(created != a && created != b);
  }
  // Exercise each committing callback separately, with conflicting selections.
  for (int storageOnly = 0; storageOnly < 2; ++storageOnly)
  {
    for (int join = 0; join < 2; ++join)
    {
      Fixture edge(2, "a");
      const ItemId a = edge.lines.at(0).id, b = edge.lines.at(1).id;
      id viewDelegate = [edge.view delegate];
      id storageDelegate = [[edge.view textStorage] delegate];
      [edge.view setDelegate:nil];
      [edge.view setSelectedRange:NSMakeRange(2, 0)];
      [edge.view setDelegate:viewDelegate];
      if (storageOnly)
      {
        edge.context->handleSelectionDidChange();
        LOKA_VERIFY(edge.cursor.state()->get() == LineCursor(b, 0));
        [edge.view setDelegate:nil];
        [edge.view setSelectedRange:NSMakeRange(0, 0)];
        edge.context->captureSelection();
      }
      else
      {
        // The view hint must come from native selection, not the older cursor.
        LOKA_VERIFY(edge.cursor.state()->get() == LineCursor(a, 1));
        [[edge.view textStorage] setDelegate:nil];
        edge.context->captureSelection();
      }
      [[edge.view textStorage] replaceCharactersInRange:NSMakeRange(join ? 1 : 2, join ? 1 : 0)
                                            withString:join ? @"" : @"\n"];
      if (!storageOnly)
      {
        [edge.view setSelectedRange:NSMakeRange(join ? 1 : 3, 0)];
        edge.notify();
      }
      [edge.view setDelegate:viewDelegate];
      [[edge.view textStorage] setDelegate:storageDelegate];
      LOKA_VERIFY(Access::restores(*edge.context) == 0);
      LOKA_VERIFY(edge.lines.at(0).id == a);
      if (join)
      {
        LOKA_VERIFY(edge.lines.size() == 1 && bytes(edge.lines.at(0).value) == "aa");
        LOKA_VERIFY(edge.lines.find(b) == -1);
        LOKA_VERIFY(edge.cursor.state()->get() == LineCursor(a, 1));
      }
      else
      {
        LOKA_VERIFY(edge.lines.size() == 3 && bytes(edge.lines.at(0).value) == "a");
        LOKA_VERIFY(edge.lines.at(1).id == b && bytes(edge.lines.at(1).value).empty());
        LOKA_VERIFY(edge.lines.at(2).id != a && edge.lines.at(2).id != b);
        LOKA_VERIFY(bytes(edge.lines.at(2).value) == "a");
        LOKA_VERIFY(edge.cursor.state()->get() == LineCursor(edge.lines.at(2).id, 0));
      }
    }
  }
  Fixture f;
  Observer observer(f);
  const ItemId first = f.lines.at(0).id, second = f.lines.at(1).id;
  const String unchanged = f.lines.at(2).value;
  [f.view insertText:@"x" replacementRange:[f.view selectedRange]];
  const bool firstKeySucceeded = bytes(f.lines.at(0).value) == "abxcd"
                                 && f.cursor.state()->get() == LineCursor(first, 3) && observer.calls == 1
                                 && NSEqualRanges([f.view selectedRange], NSMakeRange(3, 0));
  if (!firstKeySucceeded)
    printEditState(f, "first keystroke failure");
  LOKA_VERIFY(firstKeySucceeded);
  LOKA_VERIFY(observer.calls == 1);
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_UPDATE);
  LOKA_VERIFY(f.lines.at(0).id == first && f.lines.at(1).id == second);
  LOKA_VERIFY(f.lines.at(2).value.compare(unchanged, false) == StringCompareEqual);
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd" && f.cursor.state()->get() == LineCursor(first, 3));
  f.edit(@"abx\ncd\nabcd\nabcd", 4);
  LOKA_VERIFY(observer.calls == 2 && f.lines.size() == 4);
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
  LOKA_VERIFY(f.lines.at(2).id == second && f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 0));
  f.edit(@"abxcd\nabcd\nabcd", 3);
  LOKA_VERIFY(observer.calls == 3 && f.lines.size() == 3);
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
  LOKA_VERIFY(f.lines.at(1).id == second && f.cursor.state()->get() == LineCursor(first, 3));
  [f.view setSelectedRange:NSMakeRange(7, 0)];
  [[f.view delegate]
      textViewDidChangeSelection:[NSNotification notificationWithName:NSTextViewDidChangeSelectionNotification
                                                               object:f.view]];
  LOKA_VERIFY(observer.calls == 3 && f.cursor.state()->get() == LineCursor(second, 1));
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

  // Structural notifications also use text, including endpoint splits whose
  // unchanged source is consumed by the prefix/suffix comparison.
  NSString *splits[] = {@"\nabcd\nabcd\nabcd", @"abcd\n\nabcd\nabcd", @"ab\ncd\nabcd\nabcd"};
  for (unsigned i = 0; i < sizeof(splits) / sizeof(splits[0]); ++i)
  {
    Fixture structural;
    [structural.view setSelectedRange:NSMakeRange(10, 0)];
    structural.edit(splits[i], 0);
    LOKA_VERIFY(structural.lines.size() == 4);
    LOKA_VERIFY([[structural.view string] isEqualToString:splits[i]]);
    [structural.view setSelectedRange:NSMakeRange(11, 0)];
    structural.edit(@"abcd\nabcd\nabcd", 0);
    LOKA_VERIFY(structural.lines.size() == 3);
    for (unsigned short line = 0; line < structural.lines.size(); ++line)
      LOKA_VERIFY(bytes(structural.lines.at(line).value) == "abcd");
    LOKA_VERIFY(Access::restores(*structural.context) == 0 && [structural.view isEditable]);
  }
}

void testMacTextEditorUndoLocation()
{
  Fixture f(12);
  Observer observer(f);
  const ItemId first = f.lines.at(0).id, distant = f.lines.at(10).id;
  [f.view setAllowsUndo:YES];
  NSUndoManager *undo = [f.view undoManager];
  LOKA_VERIFY(undo != nil && [undo isUndoRegistrationEnabled]);
  // Manual grouping only: an event group would be closed by the run loop after
  // the test already closed it, and NSUndoManager raises on the extra end.
  [undo setGroupsByEvent:NO];
  [undo beginUndoGrouping];
  [f.view insertText:@"x" replacementRange:[f.view selectedRange]];
  [undo endUndoGrouping];
  LOKA_VERIFY([undo canUndo]);
  if (bytes(f.lines.at(0).value) != "abxcd" || f.cursor.state()->get() != LineCursor(first, 3) || observer.calls != 1)
    printEditState(f, "undo setup keystroke failure");
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd" && f.cursor.state()->get() == LineCursor(first, 3));
  LOKA_VERIFY(observer.calls == 1);
  [f.view setSelectedRange:NSMakeRange(51, 0)];
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(distant, 0));
  // The fixture has not yielded to an event boundary. Close any remaining
  // group, including the outer group supplied by event grouping.
  while ([undo groupingLevel] > 0)
    [undo endUndoGrouping];
  LOKA_VERIFY([undo groupingLevel] == 0 && [undo canUndo]);
  [undo undo];
  // The cursor fact follows the native caret wherever AppKit leaves it after
  // undo (the ruling makes the caret a reported fact, not a decision here).
  const NSUInteger undoCaret = [f.view selectedRange].location;
  NSString *undoPrefix = [[f.view string] substringToIndex:undoCaret];
  const NSRange lastBreak = [undoPrefix rangeOfString:@"\n" options:NSBackwardsSearch];
  const NSUInteger undoLine = [[undoPrefix componentsSeparatedByString:@"\n"] count] - 1;
  const NSUInteger undoColumn = undoCaret - (lastBreak.location == NSNotFound ? 0 : lastBreak.location + 1);
  const bool undoSucceeded =
      bytes(f.lines.at(0).value) == "abcd" && observer.calls == 2
      && f.cursor.state()->get()
             == LineCursor(f.lines.at(static_cast<unsigned short>(undoLine)).id, static_cast<int>(undoColumn))
      && Access::restores(*f.context) == 0 && [f.view isEditable];
  if (!undoSucceeded)
  {
    printUndoState(f, undo, "failure before restore turn");
    f.turn();
    printUndoState(f, undo, "failure after restore turn");
  }
  LOKA_VERIFY(undoSucceeded);
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abcd");
  LOKA_VERIFY(observer.calls == 2);
  LOKA_VERIFY(f.cursor.state()->get()
              == LineCursor(f.lines.at(static_cast<unsigned short>(undoLine)).id, static_cast<int>(undoColumn)));
  LOKA_VERIFY(Access::restores(*f.context) == 0 && [f.view isEditable]);

  // Also pin a notification before native selection returns to the edit.
  [f.view setSelectedRange:NSMakeRange(50, 0)];
  NSMutableString *text = [[[f.view string] mutableCopy] autorelease];
  [text insertString:@"x" atIndex:2];
  f.edit(text, 51);
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd");
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(distant, 0));
  LOKA_VERIFY(observer.calls == 3 && [f.view isEditable]);
  LOKA_VERIFY(Access::restores(*f.context) == 0);

  // Undo restores several removed rows through the storage delegate range door.
  Fixture multi;
  Observer multiObserver(multi);
  const ItemId retained = multi.lines.at(0).id;
  [multi.view setAllowsUndo:YES];
  NSUndoManager *multiUndo = [multi.view undoManager];
  LOKA_VERIFY(multiUndo != nil && [multiUndo isUndoRegistrationEnabled]);
  [multiUndo setGroupsByEvent:NO];
  [multi.view setSelectedRange:NSMakeRange(1, 12)];
  [multiUndo beginUndoGrouping];
  [multi.view insertText:@"X" replacementRange:[multi.view selectedRange]];
  [multiUndo endUndoGrouping];
  LOKA_VERIFY(multi.lines.size() == 1 && bytes(multi.lines.at(0).value) == "aXd");
  LOKA_VERIFY(multi.cursor.state()->get() == LineCursor(retained, 2) && multiObserver.calls == 1);
  [multi.view setSelectedRange:NSMakeRange(0, 0)];
  while ([multiUndo groupingLevel] > 0)
    [multiUndo endUndoGrouping];
  LOKA_VERIFY([multiUndo canUndo]);
  [multiUndo undo];
  LOKA_VERIFY(multi.lines.size() == 3 && multi.lines.at(0).id == retained);
  for (unsigned short i = 0; i < multi.lines.size(); ++i)
    LOKA_VERIFY(bytes(multi.lines.at(i).value) == "abcd");
  LOKA_VERIFY([[multi.view string] isEqualToString:@"abcd\nabcd\nabcd"]);
  // Undo reaches the rail through the storage delegate only, and AppKit leaves
  // a selection outside the restored span where it was (offset 0 here), so no
  // selection notification follows. The storage path derives the caret from
  // the change span: the end of the restored text, "abcd\nabcd\nabc|d".
  LOKA_VERIFY(NSEqualRanges([multi.view selectedRange], NSMakeRange(0, 0)));
  LOKA_VERIFY(multi.cursor.state()->get() == LineCursor(multi.lines.at(2).id, 3));
  LOKA_VERIFY(multiObserver.calls == 2 && Access::restores(*multi.context) == 0 && [multi.view isEditable]);
  // The next selection notification reports the native caret over the committed text.
  [[multi.view delegate]
      textViewDidChangeSelection:[NSNotification notificationWithName:NSTextViewDidChangeSelectionNotification
                                                               object:multi.view]];
  LOKA_VERIFY(multi.cursor.state()->get() == LineCursor(retained, 0) && multiObserver.calls == 2);
}

void testMacTextEditorStorageChanges()
{
  Fixture f;
  Observer observer(f);
  // Disable the view delegate: the storage callback alone must commit.
  id delegate = [f.view delegate];
  [f.view setDelegate:nil];
  [[f.view textStorage] replaceCharactersInRange:NSMakeRange(2, 0) withString:@"x"];
  [f.view setDelegate:delegate];
  if (bytes(f.lines.at(0).value) != "abxcd" || f.cursor.state()->get() != LineCursor(f.lines.at(0).id, 3))
    printEditState(f, "storage insertion failure");
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 3));
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd");
  LOKA_VERIFY([[f.view string] isEqualToString:@"abxcd\nabcd\nabcd"]);
  LOKA_VERIFY(observer.calls == 1 && [f.view isEditable]);
  LOKA_VERIFY(Access::restores(*f.context) == 0);
  // Only textDidChange can publish this differing caret: selection callbacks
  // are disconnected, and the committed text is already identical.
  [f.view setDelegate:nil];
  [f.view setSelectedRange:NSMakeRange(1, 0)];
  [f.view setDelegate:delegate];
  f.notify();
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 1));
  const Snapshot committed(f);
  f.notify();
  f.turn();
  committed.unchanged(f);
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 1));
  LOKA_VERIFY(observer.calls == 1 && [f.view isEditable]);
  LOKA_VERIFY(Access::restores(*f.context) == 0);

  // A replacement at a different location gets its caret from the text diff.
  [f.view setDelegate:nil];
  [[f.view textStorage] replaceCharactersInRange:NSMakeRange(2, 2) withString:@"YZQ"];
  [f.view setDelegate:delegate];
  if (bytes(f.lines.at(0).value) != "abYZQd" || f.cursor.state()->get() != LineCursor(f.lines.at(0).id, 5))
    printEditState(f, "storage replacement failure");
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abYZQd");
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 5));
  LOKA_VERIFY(observer.calls == 2 && Access::restores(*f.context) == 0);

  // Storage-only multi-line edits must derive the caret before AppKit updates
  // selection, including a caret on a newly inserted row.
  NSString *insertions[] = {@"X", @"", @"\n", @"one\ntwo\nthree"};
  for (unsigned i = 0; i < sizeof(insertions) / sizeof(insertions[0]); ++i)
  {
    Fixture multi;
    Observer multiObserver(multi);
    const ItemId first = multi.lines.at(0).id;
    id viewDelegate = [multi.view delegate];
    [multi.view setDelegate:nil];
    [multi.view setSelectedRange:NSMakeRange(0, 0)];
    [[multi.view textStorage] replaceCharactersInRange:NSMakeRange(1, 12) withString:insertions[i]];
    [multi.view setDelegate:viewDelegate];
    LOKA_VERIFY(multi.lines.size() == (i < 2 ? 1 : i == 2 ? 2 : 3));
    LOKA_VERIFY(multi.lines.at(0).id == first);
    LOKA_VERIFY(bytes(multi.lines.at(0).value) == (i == 0 ? "aXd" : i == 1 ? "ad" : i == 2 ? "a" : "aone"));
    if (i >= 2)
      LOKA_VERIFY(bytes(multi.lines.at(multi.lines.size() - 1).value) == (i == 2 ? "d" : "threed"));
    LOKA_VERIFY(multi.cursor.state()->get()
                == LineCursor(multi.lines.at(multi.lines.size() - 1).id,
                              i == 0   ? 2
                              : i == 1 ? 1
                              : i == 2 ? 0
                                       : 5));
    LOKA_VERIFY(multiObserver.calls == 1 && Access::restores(*multi.context) == 0);
    LOKA_VERIFY([multi.view isEditable]);
  }
}

void testMacTextEditorStorageAttributes()
{
  Fixture f;
  Observer observer(f);
  const Snapshot committed(f);
  // Leave a native-only selection to discriminate ignoring attributes from
  // incorrectly routing them through even the unchanged-text caret path.
  id delegate = [f.view delegate];
  [f.view setDelegate:nil];
  [f.view setSelectedRange:NSMakeRange(1, 0)];
  NSTextStorage *storage = [f.view textStorage];
  [storage beginEditing];
  [storage addAttribute:NSForegroundColorAttributeName value:[NSColor redColor] range:NSMakeRange(0, 1)];
  [storage endEditing];
  [f.view setDelegate:delegate];
  committed.unchanged(f);
  f.turn();
  committed.unchanged(f);
  LOKA_VERIFY(observer.calls == 0 && [f.view isEditable]);
  LOKA_VERIFY(Access::restores(*f.context) == 0);
}

void testMacTextEditorMultilinePasteRefusal()
{
  // Keep the registered entry point; the former refusal cases now accept.
  for (int sameCount = 0; sameCount < 2; ++sameCount)
  {
    Fixture f;
    Observer observer(f);
    const ItemId first = f.lines.at(0).id, second = f.lines.at(1).id;
    [f.view setSelectedRange:NSMakeRange(1, sameCount ? 12 : 2)];
    // Use the native replacement path used by plain-text paste, without
    // changing the user's global pasteboard.
    [f.view insertText:@"one\ntwo\nthree" replacementRange:[f.view selectedRange]];
    LOKA_VERIFY(f.lines.size() == (sameCount ? 3 : 5));
    LOKA_VERIFY(f.lines.at(0).id == first && bytes(f.lines.at(0).value) == "aone");
    LOKA_VERIFY(bytes(f.lines.at(1).value) == "two" && bytes(f.lines.at(2).value) == "threed");
    if (!sameCount)
      LOKA_VERIFY(f.lines.at(3).id == second && bytes(f.lines.at(4).value) == "abcd");
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(2).id, 5));
    LOKA_VERIFY(observer.calls == 1 && f.lines.revision().get().change.kind == LIST_BATCH);
    LOKA_VERIFY([f.view isEditable] && Access::restores(*f.context) == 0);
  }
  for (int action = 0; action < 3; ++action)
  {
    Fixture f;
    Observer observer(f);
    const ItemId first = f.lines.at(0).id;
    [f.view setSelectedRange:NSMakeRange(1, 12)];
    if (action == 0)
      [f.view insertText:@"X" replacementRange:[f.view selectedRange]];
    else if (action == 1)
      [f.view deleteBackward:nil];
    else
      [f.view insertNewline:nil];
    LOKA_VERIFY(f.lines.size() == (action == 2 ? 2 : 1));
    LOKA_VERIFY(f.lines.at(0).id == first);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == (action == 0 ? "aXd" : action == 1 ? "ad" : "a"));
    if (action == 2)
      LOKA_VERIFY(bytes(f.lines.at(1).value) == "d");
    LOKA_VERIFY(f.cursor.state()->get()
                == (action == 2 ? LineCursor(f.lines.at(1).id, 0) : LineCursor(first, action == 0 ? 2 : 1)));
    LOKA_VERIFY(observer.calls == 1 && f.lines.revision().get().change.kind == LIST_BATCH);
    LOKA_VERIFY([f.view isEditable] && Access::restores(*f.context) == 0);
  }
  // A view-only notification reports its final selection, which deliberately
  // differs from the diff-derived insertion end on the newly inserted row.
  Fixture viewOnly;
  Observer observer(viewOnly);
  id storageDelegate = [[viewOnly.view textStorage] delegate];
  [[viewOnly.view textStorage] setDelegate:nil];
  viewOnly.edit(@"aone\ntwo\nthreed", 0);
  [[viewOnly.view textStorage] setDelegate:storageDelegate];
  LOKA_VERIFY(viewOnly.lines.size() == 3 && bytes(viewOnly.lines.at(2).value) == "threed");
  LOKA_VERIFY(viewOnly.cursor.state()->get() == LineCursor(viewOnly.lines.at(0).id, 0));
  LOKA_VERIFY(observer.calls == 1 && Access::restores(*viewOnly.context) == 0);
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
    [undo setGroupsByEvent:NO];
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
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 3));
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
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 1));
    f.restored(1);
    LOKA_VERIFY(NSEqualRanges([f.view selectedRange], NSMakeRange(1, 0)));
    nextKey(f);
  }
}

void testMacTextEditorReplacementFailure()
{
  {
    Fixture empty(1, "");
    empty.edit(@"\u00e9", 1);
    empty.restored(1);
  }
  Fixture f;
  LokaRefusingEditorView *view = [[LokaRefusingEditorView alloc] initWithFrame:[f.view frame]];
  [view setString:[f.view string]];
  [view setSelectedRange:[f.view selectedRange]];
  [view setDelegate:[f.view delegate]];
  [[view textStorage] setDelegate:[[f.view textStorage] delegate]];
  [f.view setDelegate:nil];
  [[f.view textStorage] setDelegate:nil];
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
  {
    Highlighter policy;
    Fixture pending;
    pending.node.props.highlighter(policy);
    pending.context->onPropsApplied();
    id delegate = [pending.view delegate];
    [pending.view setDelegate:nil];
    [[pending.view textStorage] replaceCharactersInRange:NSMakeRange(2, 0) withString:@"x"];
    [pending.view setDelegate:delegate];
    LOKA_VERIFY(policy.calls == 3);
    // The owner changes structure before the queued native style pass.
    LOKA_VERIFY(pending.lines.insert(0, String("owner")) == EDIT_OK);
    for (int i = 0; i < 20 && Access::restores(*pending.context) == 0; ++i)
      pending.turn();
    LOKA_VERIFY(Access::restores(*pending.context) == 1);
    LOKA_VERIFY([[pending.view string] isEqualToString:@"owner\nabxcd\nabcd\nabcd"]);
    LOKA_VERIFY(policy.calls == 5);
  }
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
  verifyFont(styled, (NSFont *)f.controller.textFont(Bold + Italic + FontSize<18>(), true));
  highlighter.fail = true;
  f.edit(@"abxycd\nabcd\nabcd", 4);
  LOKA_VERIFY(highlighter.calls == 5 && bytes(f.lines.at(0).value) == "abxycd");
  verifyFont([[f.view textStorage] attribute:NSFontAttributeName atIndex:0 effectiveRange:0],
             (NSFont *)f.controller.textFont(TextStyle(), true));
  highlighter.fail = false;
  loka::core::testing::failLokaAllocRaw("AttributedString", "Segments", 1);
  f.edit(@"abxyQcd\nabcd\nabcd", 5);
  loka::core::testing::allowLokaAllocRaw();
  LOKA_VERIFY(highlighter.calls == 6 && bytes(f.lines.at(0).value) == "abxyQcd");
  verifyFont([[f.view textStorage] attribute:NSFontAttributeName atIndex:0 effectiveRange:0],
             (NSFont *)f.controller.textFont(TextStyle(), true));
  // Storage-only input queues one style pass and never styles in the delegate.
  id delegate = [f.view delegate];
  [f.view setDelegate:nil];
  NSTextStorage *storage = [f.view textStorage];
  [storage replaceCharactersInRange:NSMakeRange(2, 0) withString:@"R"];
  [storage replaceCharactersInRange:NSMakeRange(3, 0) withString:@"S"];
  [f.view setDelegate:delegate];
  LOKA_VERIFY(highlighter.calls == 6);
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abRSxyQcd");
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 4));
  // A run-loop slice may service another source before the queued selector.
  // Both edits coalesce; the line cache skips the two unchanged lines, even
  // if AppKit also delivers a view notification.
  NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:2.0];
  while (highlighter.calls < 7 && [deadline timeIntervalSinceNow] > 0)
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
  LOKA_VERIFY(highlighter.calls == 7);
  verifyFont([storage attribute:NSFontAttributeName atIndex:2 effectiveRange:0],
             (NSFont *)f.controller.textFont(Bold + Italic + FontSize<18>(), true));
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 4));
  f.turn();
  LOKA_VERIFY(highlighter.calls == 7);

  // A queued style pass loses its owner on retained detach.
  [f.view setDelegate:nil];
  [storage replaceCharactersInRange:NSMakeRange(2, 2) withString:@""];
  [f.view setDelegate:delegate];
  LOKA_VERIFY(highlighter.calls == 7);
  NotifySubtreeNodeDetached(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  f.turn();
  LOKA_VERIFY(highlighter.calls == 7);
  NotifySubtreeNodeAttached(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  LOKA_VERIFY(highlighter.calls == 10);

  // Pending restore is invalidated by retained detach, as is the style cache.
  f.edit(@"\u00e9", 1);
  NotifySubtreeNodeDetached(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  LOKA_VERIFY([f.scroll superview] == nil && [f.view delegate] == nil);
  LOKA_VERIFY([[f.view textStorage] delegate] == nil);
  f.turn();
  LOKA_VERIFY(Access::restores(*f.context) == 0);
  NotifySubtreeNodeAttached(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  LOKA_VERIFY([f.scroll superview] == [f.host.window contentView] && [f.view delegate] != nil);
  LOKA_VERIFY((id)[[f.view textStorage] delegate] == (id)[f.view delegate]);
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
  LOKA_VERIFY([[f.view textStorage] delegate] == nil);
}
