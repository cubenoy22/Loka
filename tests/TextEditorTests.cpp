#include "support/LifecycleFactTestAccess.hpp"
#include "support/TextEditorStateOwner.hpp"
#include "support/TextEditorAccess.hpp"
#include "support/TextEditorReportRefusal.hpp"
#include "TextEditorTests.hpp"
#include "app/nodes/controls/TextChangeSpan.hpp"
#include "app/nodes/controls/TextEditorDiff.hpp"
#include "support/TextEditorContractSnapshot.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include "platform/null/context/NullTextEditorContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/projection/CollectPaintAnswers.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "platform/StringUTF8.hpp"
#include "support/LokaAllocFailure.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>
#include <vector>
namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  typedef loka::testing::TextEditorInput Input;
  struct Fixture : loka::app::testing::TextEditorStateOwner
  {
    ObservableList<String> lines;
    NullScenePlatformController platform;
    TextEditorNode node;
    NullTextEditorContext *context;
    explicit Fixture(unsigned short count = 3, const std::string &text = "abcd", unsigned short capacity = 256)
        : lines(),
          platform(),
          node(TextEditorProps(lines, cursor).moveCaretTo(request)),
          context(0)
    {
      this->node.setPropsTypeId(TextEditorProps::staticTypeId());
      LOKA_VERIFY(this->lines.attach(&this->tracker, capacity) == ATTACH_OK);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(this->lines.insert(i, String(text)) == EDIT_OK);
      if (count)
      {
        StateTrackerGuard guard(&this->tracker);
        this->request.set(LineCursor(this->lines.at(0).id, 2));
      }
      LayoutState bounds;
      bounds.width = 200;
      bounds.height = 300;
      bounds.spacing = 4;
      this->platform.projectLayoutForTesting(&this->node, bounds);
      this->context = static_cast<NullTextEditorContext *>(this->node.getContext());
      LOKA_VERIFY(this->context);
    }
  };
  typedef loka::testing::TextEditorContractSnapshot Snapshot;
  struct Observer
  {
    Fixture &fixture;
    unsigned immediate, settled, cursorNotifications;
    LineCursor expected;
    EditorResult nested;
    bool reenter;
    bool reenterDeferred;
    explicit Observer(Fixture &f)
        : fixture(f),
          immediate(0),
          settled(0),
          cursorNotifications(0),
          expected(),
          nested(EDITOR_OK),
          reenter(false),
          reenterDeferred(false)
    {
      const_cast<State<ListRevision> &>(f.lines.revision()).bind(&changed, this, false);
      f.cursor.state()->bind(&cursorChanged, this, false);
    }
    static void cursorChanged(void *data)
    {
      ++static_cast<Observer *>(data)->cursorNotifications;
    }
    ~Observer()
    {
      fixture.cursor.state()->unbind(&cursorChanged, this);
      const_cast<State<ListRevision> &>(fixture.lines.revision()).unbind(&changed, this);
    }
    static void changed(void *data)
    {
      Observer &self = *static_cast<Observer *>(data);
      ++self.immediate;
      self.fixture.tracker.defer(&flushed, &self);
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(self.fixture.node)
                      .moveCaret(self.fixture.cursor.state()->get())
                  == EDITOR_REENTRANT);
      const LineCursor caret(self.fixture.lines.at(0).id, 0);
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(self.fixture.node).applyReplace(caret, caret, "", 0)
                  == EDITOR_REENTRANT);
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(self.fixture.node)
                      .applyReplace(caret, caret, "", 0, RowCursor::None())
                  == EDITOR_REENTRANT);
      if (self.reenter)
      {
        self.reenter = false;
        Snapshot snapshot(self.fixture);
        self.nested = Input::type(*self.fixture.context, 'Z');
        snapshot.unchanged(self.fixture);
      }
    }
    static void flushed(void *data)
    {
      Observer &self = *static_cast<Observer *>(data);
      ++self.settled;
      if (!self.expected.isNone())
        LOKA_VERIFY(self.fixture.cursor.state()->get() == self.expected);
      if (self.reenterDeferred)
      {
        self.reenterDeferred = false;
        Snapshot snapshot(self.fixture);
        self.nested = Input::type(*self.fixture.context, 'Z');
        snapshot.unchanged(self.fixture);
      }
    }
  };
  std::string bytes(const String &value)
  {
    std::string out;
    LOKA_VERIFY(loka::platform::CollectUtf8(value, out));
    return out;
  }
  void restored(Fixture &f, const Snapshot &snapshot, const Observer &observer, unsigned restores)
  {
    snapshot.unchanged(f);
    LOKA_VERIFY(observer.immediate == 0 && observer.settled == 0 && observer.cursorNotifications == 0);
    std::string projection;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).project(projection) == EDITOR_OK);
    LOKA_VERIFY(Input::buffer(*f.context) == projection);
    LOKA_VERIFY(Input::restores(*f.context) == restores);
    if (f.lines.find(snapshot.cursor.line) >= 0)
      LOKA_VERIFY(Input::caret(*f.context) == snapshot.cursor);
  }
  class Highlighter : public LineHighlighter
  {
  public:
    mutable unsigned calls;
    Highlighter()
        : calls(0)
    {
    }
    virtual bool highlight(const String &line, AttributedString::Builder &out) const
    {
      ++this->calls;
      return out.append(line, Bold);
    }
  };
} // namespace
void testTextEditorActions()
{
  // Column diffs are independent of native selection and storage edit unions.
  struct SpanCase { const char *before; const char *after; unsigned start, oldEnd, newEnd; };
  const SpanCase spanCases[] = {
      {"abcd", "abxcd", 2, 2, 3}, {"abxcd", "abcd", 2, 3, 2},
      {"abxcd", "abYZQd", 2, 4, 5}, {"abcd", "abXYd", 2, 3, 4},
      {"aaaa", "aaa", 3, 4, 3}, {"", "x", 0, 0, 1},
      {"x", "", 0, 1, 0}, {"abcd", "abcd", 4, 4, 4},
      {"abcd", "Xbcd", 0, 1, 1}, {"abcd", "abcdX", 4, 4, 5}};
  for (unsigned i = 0; i < sizeof(spanCases) / sizeof(spanCases[0]); ++i)
  {
    const std::string before(spanCases[i].before), after(spanCases[i].after);
    const loka::app::detail::TextChangeSpan span(before, before.size(), after, after.size());
    LOKA_VERIFY(span.start() == spanCases[i].start);
    LOKA_VERIFY(span.beforeEnd() == spanCases[i].oldEnd);
    LOKA_VERIFY(span.afterEnd() == spanCases[i].newEnd);
  }
  // Shared range detection runs on Linux; an absent hint preserves text-only detection.
  const struct DiffCase
  {
    const char *before;
    const char *after;
    int first, oldCount, newCount;
  } cases[] = {{"aQcd\rabcd\rabcd", "abxcd\rabcd\rabcd", 0, 1, 1},
               {"a\rb", "a\rb", 2, 0, 0},
               {"abcd", "ab\rcd", 0, 1, 2},
               {"ab\rcd", "abcd", 0, 2, 1},
               {"abcd", "abcd\r", 0, 1, 2},
               {"abcd\r", "abcd", 0, 2, 1},
               {"abcd", "\rabcd", 0, 1, 2},
               {"\rabcd", "abcd", 0, 2, 1},
               {"", "\r", 0, 1, 2},
               {"a\rb\rc", "A\rb\rC", 0, 3, 3},
               {"a\rb\rc", "a\rB\rC", 1, 2, 2},
               {"a", "aQ\rR\rS", 0, 1, 3}};
  for (std::size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    const loka::app::TextEditorLineDiff diff = loka::app::DiffTextEditorLines(cases[i].before, cases[i].after);
    LOKA_VERIFY(diff.first() == cases[i].first && diff.before() == cases[i].oldCount
                && diff.after() == cases[i].newCount);
  }
  const std::string atCap = std::string(2, 'a') + 'x' + std::string(8189, 'a');
  const loka::app::TextEditorLineDiff capDiff = loka::app::DiffTextEditorLines(std::string(8191, 'a'), atCap);
  LOKA_VERIFY(capDiff.first() == 0 && capDiff.before() == 1 && capDiff.after() == 1);
  LOKA_VERIFY(loka::app::TextEditorLogicalLine(atCap, 0).size() == 8192);

  // Identity pins consume the pure diff through the document's replacement door.
  for (unsigned short count = 2; count <= 3; ++count)
  {
    for (int edge = 0; edge < 2; ++edge)
    {
      Fixture split(count, "a");
      const Snapshot original(split);
      const std::string before = count == 2 ? "a\ra" : "a\ra\ra";
      const std::string after = count == 2 ? "a\r\ra" : "a\r\ra\ra";
      const int line = edge == 0 ? 1 : 0, column = edge == 0 ? 0 : 1;
      const TextEditorLineDiff diff = DiffTextEditorLines(before, after, line, column);
      LOKA_VERIFY(diff.first() == line && diff.before() == 1 && diff.after() == 2);
      const LineCursor caret(split.lines.at(static_cast<unsigned short>(diff.first())).id,
                             static_cast<LineCursor::Column>(TextEditorLogicalLine(after, diff.first()).size()));
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(split.node).applyReplace(caret, caret, "\r", 1)
                  == EDITOR_OK);
      LOKA_VERIFY(split.lines.size() == count + 1);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(split.lines.at(i <= line ? i : i + 1).id == original.ids[i]);
      const ItemId added = split.lines.at(static_cast<unsigned short>(line + 1)).id;
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(added != original.ids[i]);
      LOKA_VERIFY(bytes(split.lines.at(static_cast<unsigned short>(line)).value) == (column ? "a" : ""));
      LOKA_VERIFY(bytes(split.lines.at(static_cast<unsigned short>(line + 1)).value) == (column ? "" : "a"));
      LOKA_VERIFY(split.cursor.state()->get() == LineCursor(added, 0));
      std::string projected;
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(split.node).project(projected) == EDITOR_OK
                  && projected == after);
    }
    // Nonempty identical lines, and ambiguous runs of empty identical lines.
    for (int empty = 0; empty < 2; ++empty)
    {
      Fixture join(count, empty ? "" : "a");
      const Snapshot original(join);
      const std::string before = empty ? std::string(count - 1, '\r') : (count == 2 ? "a\ra" : "a\ra\ra");
      const std::string after = empty ? std::string(count - 2, '\r') : (count == 2 ? "aa" : "aa\ra");
      const TextEditorLineDiff diff = DiffTextEditorLines(before, after, 1, 0);
      LOKA_VERIFY(diff.first() == 0 && diff.before() == 2 && diff.after() == 1);
      const LineCursor previous(join.lines.at(static_cast<unsigned short>(diff.first())).id,
                                static_cast<LineCursor::Column>(TextEditorLogicalLine(before, diff.first()).size()));
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(join.node).applyReplace(
                      previous, LineCursor(join.lines.at(static_cast<unsigned short>(diff.first() + 1)).id, 0), "", 0)
                  == EDITOR_OK);
      LOKA_VERIFY(join.lines.size() == count - 1 && join.lines.at(0).id == original.ids[0]);
      LOKA_VERIFY(join.lines.find(original.ids[1]) < 0);
      if (count == 3)
        LOKA_VERIFY(join.lines.at(1).id == original.ids[2]);
      LOKA_VERIFY(join.cursor.state()->get() == LineCursor(original.ids[0], empty ? 0 : 1));
      std::string projected;
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(join.node).project(projected) == EDITOR_OK
                  && projected == after);
    }
  }
  const TextEditorLineDiff distant = DiffTextEditorLines("aQcd\rabcd\rabcd", "abxcd\rabcd\rabcd", 2, 0);
  LOKA_VERIFY(distant.first() == 0 && distant.before() == 1 && distant.after() == 1);

  Fixture f;
  RefusedNodeHandler foreignHandler(NodeTypeToken<TextEditorNode>());
  LOKA_VERIFY(!f.platform.registerNodeHandler(&foreignHandler));
  const ItemId first = f.lines.at(0).id, second = f.lines.at(1).id, third = f.lines.at(2).id;
  Observer observer(f);
  observer.expected = LineCursor(first, 3);
  const ListRevision initial = f.lines.revision().get();
  LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd");
  LOKA_VERIFY(f.lines.revision().get().content == initial.content + 1);
  LOKA_VERIFY(f.lines.revision().get().structure == initial.structure);
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_UPDATE);
  LOKA_VERIFY(observer.immediate == 1 && observer.settled == 1);
  observer.expected = LineCursor::None();
  const unsigned beforeSplit = observer.immediate;
  LOKA_VERIFY(Input::enter(*f.context) == EDITOR_OK);
  LOKA_VERIFY(observer.immediate == beforeSplit + 1);
  LOKA_VERIFY(f.lines.size() == 4 && bytes(f.lines.at(0).value) == "abx" && bytes(f.lines.at(1).value) == "cd");
  LOKA_VERIFY(f.lines.at(0).id == first && f.lines.at(2).id == second && f.lines.at(3).id == third);
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 0));
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
  LOKA_VERIFY(f.lines.revision().get().structure == initial.structure + 1);
  const unsigned beforeJoin = observer.immediate;
  LOKA_VERIFY(Input::backspace(*f.context) == EDITOR_OK);
  LOKA_VERIFY(observer.immediate == beforeJoin + 1);
  LOKA_VERIFY(f.lines.size() == 3 && f.lines.at(1).id == second && f.lines.at(2).id == third);
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd" && f.cursor.state()->get() == LineCursor(first, 3));
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
  const ListRevision beforeMove = f.lines.revision().get();
  LOKA_VERIFY(Input::move(*f.context, LineCursor(second, 1)) == EDITOR_OK);
  LOKA_VERIFY(!(f.lines.revision().get() != beforeMove));
  LOKA_VERIFY(Input::paste(*f.context, "Q\r\nR\nS") == EDITOR_OK);
  LOKA_VERIFY(bytes(f.lines.at(1).value) == "aQ" && bytes(f.lines.at(2).value) == "R"
              && bytes(f.lines.at(3).value) == "Sbcd");
  LOKA_VERIFY(f.lines.at(4).id == third && f.cursor.state()->get() == LineCursor(f.lines.at(3).id, 1));
  LayoutState layout;
  layout.y = 10;
  layout.height = 300;
  layout.spacing = 4;
  LOKA_VERIFY(f.context->layout(&f.platform, layout) == 94 && layout.height == 80);
  const Snapshot beforeDetach(f);
  NotifySubtreeNodeDetached(&f.node);
  LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_UNAVAILABLE);
  beforeDetach.unchanged(f);
  NotifySubtreeNodeAttached(&f.node);
  f.context->syncFromNode();
  LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
  std::printf("[pin] TextEditor update, settled cursor, one-batch split/join, paste, caret, block layout\n");
}
void testTextEditorRefusals()
{
  {
    Fixture f(256);
    Observer observer(f);
    Snapshot snapshot(f);
    LOKA_VERIFY(Input::enter(*f.context) == EDITOR_CAPACITY);
    restored(f, snapshot, observer, 1);
    LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
  }
  {
    Fixture f;
    Observer observer(f);
    LOKA_VERIFY(f.lines.detach() == EDIT_OK);
    LOKA_VERIFY(f.lines.attach(&f.tracker, 256) == ATTACH_OK);
    LOKA_VERIFY(f.lines.insert(0, String("fresh")) == EDIT_OK);
    observer.immediate = observer.settled = 0;
    Snapshot snapshot(f);
    LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_STALE_ID);
    restored(f, snapshot, observer, 1);
    LOKA_VERIFY(Input::caret(*f.context) == LineCursor(f.lines.at(0).id, 2));
    LOKA_VERIFY(Input::type(*f.context, 'y') == EDITOR_OK);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "fryesh");
  }
  // Split refusal covers seam-owned scratch; String construction keeps its rail contract (#827).
  loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 0);
  {
    Fixture f;
    Observer observer(f);
    Snapshot snapshot(f);
    loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 1);
    LOKA_VERIFY(Input::enter(*f.context) == EDITOR_ALLOCATION);
    restored(f, snapshot, observer, 1);
    LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
  }
  loka::core::testing::allowLokaAllocRaw();
  {
    Fixture f;
    Observer observer(f);
    Snapshot snapshot(f);
    LOKA_VERIFY(Input::type(*f.context, static_cast<char>(0x80)) == EDITOR_NON_ASCII);
    restored(f, snapshot, observer, 1);
    LOKA_VERIFY(Input::paste(*f.context, std::string(8193, 'a')) == EDITOR_CAPACITY);
    restored(f, snapshot, observer, 2);
    LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
  }
  for (int deferred = 0; deferred < 2; ++deferred)
  {
    Fixture f;
    Observer observer(f);
    observer.reenter = deferred == 0;
    observer.reenterDeferred = deferred != 0;
    LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
    LOKA_VERIFY(observer.nested == EDITOR_REENTRANT);
    LOKA_VERIFY(observer.immediate == 1 && observer.settled == 1);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd");
    LOKA_VERIFY(Input::buffer(*f.context) == "abxcd\rabcd\rabcd");
    LOKA_VERIFY(Input::caret(*f.context) == f.cursor.state()->get() && Input::restores(*f.context) == 1);
    LOKA_VERIFY(Input::type(*f.context, 'y') == EDITOR_OK);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxycd");
  }
  {
    Fixture f(257, "a", 257);
    LOKA_VERIFY(Input::status(*f.context) == EDITOR_CAPACITY);
    LOKA_VERIFY(Input::buffer(*f.context).empty());
  }
  {
    Fixture f(1, std::string(8193, 'a'));
    LOKA_VERIFY(Input::status(*f.context) == EDITOR_CAPACITY);
    LOKA_VERIFY(Input::buffer(*f.context).empty());
  }
  {
    Fixture f(1, std::string(8191, 'a'));
    LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
    LOKA_VERIFY(bytes(f.lines.at(0).value).size() == 8192);
    LOKA_VERIFY(Input::buffer(*f.context).size() == 8192);
    Observer observer(f);
    Snapshot snapshot(f);
    LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_CAPACITY);
    restored(f, snapshot, observer, 1);
  }
  {
    Fixture f(1, "");
    LOKA_VERIFY(Input::move(*f.context, LineCursor(f.lines.at(0).id, 0)) == EDITOR_OK);
    std::string normalizedPaste(7800, 'a');
    for (int i = 0; i < 255; ++i)
      normalizedPaste += "\r\n";
    LOKA_VERIFY(normalizedPaste.size() > TextEditorProps::kMaxBytes);
    LOKA_VERIFY(Input::paste(*f.context, normalizedPaste) == EDITOR_OK);
    LOKA_VERIFY(f.lines.size() == 256 && Input::buffer(*f.context).size() == 8055);
  }
  // Exhaustion is reachable without adding a production test door (16-bit seq).
  {
    Fixture f(1);
    for (unsigned int i = 1; i < 65535; ++i)
    {
      ItemId temporary;
      LOKA_VERIFY(f.lines.insert(1, String(), &temporary) == EDIT_OK);
      LOKA_VERIFY(f.lines.remove(temporary) == EDIT_OK);
    }
    Observer observer(f);
    Snapshot snapshot(f);
    LOKA_VERIFY(Input::enter(*f.context) == EDITOR_ID_EXHAUSTED);
    restored(f, snapshot, observer, 1);
    LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
  }
  {
    Fixture f;
    Observer observer(f);
    Snapshot snapshot(f);
    loka::app::testing::TextEditorStateOwner foreign;
    f.node.props = TextEditorProps(f.lines, foreign.cursor);
    const LineCursor caret(f.lines.at(0).id, 2);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(caret, caret, "\r", 1)
                == EDITOR_OWNER_MISMATCH);
    snapshot.unchanged(f);
    LOKA_VERIFY(observer.immediate == 0 && observer.cursorNotifications == 0);
    f.node.props = TextEditorProps(f.lines, f.cursor).moveCaretTo(f.request);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(caret.line, 0),
                    LineCursor(caret.line, static_cast<LineCursor::Column>(bytes(f.lines.at(0).value).size())),
                    "direct",
                    6,
                    RowCursor(0, 6))
                == EDITOR_OK);
    LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_UPDATE);
  }
  std::printf("[pin] TextEditor capacity, stale generation, split scratch allocation, ASCII, re-entry, load caps, "
              "exhausted IDs\n");
}
void testTextEditorHighlight()
{
  Highlighter highlighter;
  const String input("abc");
  const LineHighlight first(input, &highlighter);
  highlighter.calls = 0;
  const LineHighlight shared(input, &highlighter, &first);
  LOKA_VERIFY(highlighter.calls == 0 && shared.value() == first.value());
  const LineHighlight fresh(String("abc"), &highlighter, &shared);
  LOKA_VERIFY(highlighter.calls == 1 && fresh.value() == shared.value());
  LOKA_VERIFY(LineCursor::None().isNone());
}

namespace
{
  struct SceneReposter
  {
    Request<LineCursor> &request;
    State<LineCursor> &fact;
    unsigned reports;
    SceneReposter(Request<LineCursor> &r, State<LineCursor> &f)
        : request(r),
          fact(f),
          reports(0)
    {
    }
    static void changed(void *data)
    {
      SceneReposter &self = *static_cast<SceneReposter *>(data);
      ++self.reports;
      LineCursor next = self.fact.get();
      next.column = next.column == 0 ? 1 : 0;
      self.request.set(next);
    }
  };
  class EditorPresenter : public NullScenePlatformController
  {
  public:
    virtual void onChange(Node *root, NodeDirtyFlags, bool)
    {
      LayoutState bounds;
      bounds.width = 240;
      bounds.height = 320;
      this->projectLayoutForTesting(root, bounds);
    }
  };
  class EditorSceneRoot : public BoundaryNodeFor<EditorSceneRoot>
  {
  public:
    ObservableList<String> lines;
    Reported<LineCursor> cursor;
    RequestWithReply<LineCursor> request;
    explicit EditorSceneRoot(const BoundaryPropsFor<EditorSceneRoot> &p)
        : BoundaryNodeFor<EditorSceneRoot>(p)
    {
      this->declareStates(3).state(this->cursor, LineCursor::None()).state(this->request, LineCursor::None());
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
    virtual void attachNode(NodeComposition &)
    {
      StateTracker *owner = 0;
      if (this->lines.queryMutationTracker(owner) == EDIT_OK)
        return;
      LOKA_VERIFY(this->lines.attach(this->tracker()->asPushTracker(), 258) == ATTACH_OK);
      LOKA_VERIFY(this->lines.insert(0, String("hello")) == EDIT_OK);
      this->request.set(LineCursor(this->lines.at(0).id, 1));
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(TextEditor(this->lines, this->cursor).moveCaretTo(this->request));
    }
  };
} // namespace
void testTextEditorScene()
{
  EditorPresenter platform;
  Scene scene((Boundary<EditorSceneRoot>()));
  scene.mount(&platform);
  EditorSceneRoot *root = static_cast<EditorSceneRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(root);
  LayoutState layout;
  layout.width = 240;
  layout.height = 320;
  platform.projectLayoutForTesting(root, layout);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  LOKA_VERIFY(root->request.reply().isValid());
  TextEditorNode *node = static_cast<TextEditorNode *>(root->childrenHead());
  LOKA_VERIFY(node && node->nodeTypeKey() == NodeTypeToken<TextEditorNode>());
  NullTextEditorContext *context = static_cast<NullTextEditorContext *>(node->getContext());
  LOKA_VERIFY(context && Input::buffer(*context) == "hello");
  const PaintQuery query = {platform.paintScope(), PLACEMENT_ELIGIBLE};
  PaintAnswerBuffer<> answers;
  const PaintApplyVerdict verdict = CollectPaintAnswers(*root, query, answers, platform);
  LOKA_VERIFY(verdict.nativeScheduledCount() == 1);
  LOKA_VERIFY(!verdict.widened());
  PaintAnswer answer;
  LOKA_VERIFY(platform.queryPaintAnswer(node, context, query, answer));
  LOKA_VERIFY(answer.kind == PAINT_ANSWER_NATIVE_SCHEDULED);
  std::printf("[pin] TextEditor paint is native scheduled without widening\n");
  LOKA_VERIFY(Input::type(*context, 'x') == EDITOR_OK);
  LOKA_VERIFY(bytes(root->lines.at(0).value) == "hxello" && root->cursor.state()->get().column == 2);
  const LineCursor requested(root->lines.at(0).id, 4);
  root->request.set(requested);
  for (int i = 0; scene.hasPendingInvalidation() && i < 12; ++i)
    LOKA_VERIFY(scene.flushInvalidation());
  LOKA_VERIFY(root->request.get().isNone());
  LOKA_VERIFY(root->cursor.state()->get() == requested && Input::caret(*context) == requested);
  SceneReposter reposter(root->request, *root->cursor.state());
  root->cursor.state()->bind(&SceneReposter::changed, &reposter, false);
  root->request.set(LineCursor(root->lines.at(0).id, 0));
  LOKA_VERIFY(scene.flushInvalidation());
  LOKA_VERIFY(reposter.reports > 0 && !root->request.get().isNone());
  std::printf("[pin] endless repost Scene flush returned after %u reports with pending work\n", reposter.reports);
  LOKA_VERIFY(scene.hasPendingInvalidation());
  root->cursor.state()->unbind(&SceneReposter::changed, &reposter);
  for (int i = 0; scene.hasPendingInvalidation() && i < 12; ++i)
    LOKA_VERIFY(scene.flushInvalidation());
  LOKA_VERIFY(root->request.get().isNone());
  LOKA_VERIFY(root->lines.update(root->lines.at(0).id, String("external")) == EDIT_OK);
  for (int i = 0; scene.hasPendingInvalidation() && i < 12; ++i)
    LOKA_VERIFY(scene.flushInvalidation());
  LOKA_VERIFY(Input::buffer(*context) == "external");
  for (unsigned short i = 1; i < 257; ++i)
    LOKA_VERIFY(root->lines.insert(i, String()) == EDIT_OK);
  platform.projectLayoutForTesting(root, layout);
  LOKA_VERIFY(Input::status(*context) == EDITOR_CAPACITY && Input::buffer(*context).empty());
  LOKA_VERIFY(Input::type(*context, 'x') == EDITOR_CAPACITY);
  LOKA_VERIFY(root->lines.remove(root->lines.at(256).id) == EDIT_OK);
  platform.projectLayoutForTesting(root, layout);
  LOKA_VERIFY(Input::status(*context) == EDITOR_OK && Input::type(*context, 'y') == EDITOR_OK);
  loka::dsl::testing::SceneTestAccess::unmount(scene);
}
void testTextEditorReplace()
{
  {
    Fixture f(3, "ab", 3);
    const ItemId a = f.lines.at(0).id, b = f.lines.at(1).id, c = f.lines.at(2).id;
    LOKA_VERIFY(f.lines.update(b, String("cd")) == EDIT_OK);
    LOKA_VERIFY(f.lines.update(c, String("ef")) == EDIT_OK);
    Observer observer(f);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(a, 1), LineCursor(c, 1), "X\rY", 3)
                == EDITOR_OK);
    LOKA_VERIFY(f.lines.size() == 2 && f.lines.at(0).id == a);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "aX" && bytes(f.lines.at(1).value) == "Yf");
    const ItemId d = f.lines.at(1).id;
    LOKA_VERIFY(d != a && d != b && d != c && f.lines.find(b) < 0 && f.lines.find(c) < 0);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(d, 1));
    LOKA_VERIFY(observer.immediate == 1 && observer.settled == 1 && observer.cursorNotifications == 1);
    LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
    // Grow back to the reserved capacity, then refuse a final size that exceeds it.
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(a, 1), LineCursor(d, 1), "1\r2\r3", 5)
                == EDITOR_OK);
    LOKA_VERIFY(f.lines.size() == 3 && bytes(f.lines.at(2).value) == "3f");
    Snapshot snapshot(f);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(a, 0), LineCursor(a, 1), "x\ry", 3)
                == EDITOR_CAPACITY);
    snapshot.unchanged(f);
  }
  // Full capacity with a nonshrinking replacement still needs REMOVE before INSERT.
  {
    Fixture f(3, "abcd", 3);
    const ItemId a = f.lines.at(0).id, c = f.lines.at(2).id;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(a, 1), LineCursor(c, 2), "X\r\nY\nZ", 6)
                == EDITOR_OK);
    LOKA_VERIFY(f.lines.size() == 3 && f.lines.at(0).id == a);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "aX" && bytes(f.lines.at(1).value) == "Y"
                && bytes(f.lines.at(2).value) == "Zcd");
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(2).id, 1));
  }
  {
    Fixture f(1);
    const ItemId a = f.lines.at(0).id;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(a, 2), LineCursor(a, 2), "\r", 1, RowCursor(1, 1))
                == EDITOR_OK);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "ab" && bytes(f.lines.at(1).value) == "cd");
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 1));
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(a, 0), LineCursor(a, 1), "Z", 1, RowCursor::None())
                == EDITOR_OK);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor::None());
  }
  // Post-state rows before, inside, and after the replaced range use different lengths.
  for (unsigned short row = 0; row < 4; ++row)
  {
    Fixture f(4, "abcd");
    const ItemId first = f.lines.at(1).id, last = f.lines.at(2).id, tail = f.lines.at(3).id;
    const LineCursor::Column column = row == 1 ? 2 : row == 2 ? 3 : 4;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(first, 1), LineCursor(last, 2), "X\rY", 3, RowCursor(row, column))
                == EDITOR_OK);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(row).id, column));
    LOKA_VERIFY(f.lines.at(3).id == tail);
  }
  for (int grow = 0; grow < 2; ++grow)
  {
    Fixture f(4, "abcd");
    const ItemId tail = f.lines.at(3).id;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(LineCursor(f.lines.at(1).id, 0),
                                                                                    LineCursor(f.lines.at(2).id, 4),
                                                                                    grow ? "x\ry\rz" : "x",
                                                                                    grow ? 5 : 1,
                                                                                    RowCursor(grow ? 4 : 2, 4))
                == EDITOR_OK);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(tail, 4));
  }
  // Whole-line replacement accepts a post-state caret on another row or no caret.
  {
    Fixture f;
    const ItemId a = f.lines.at(0).id, c = f.lines.at(2).id;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(a, 0),
                    LineCursor(a, static_cast<LineCursor::Column>(bytes(f.lines.at(0).value).size())),
                    "x",
                    1,
                    RowCursor(static_cast<unsigned short>(f.lines.find(c)), 4))
                == EDITOR_OK);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(c, 4));
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                    LineCursor(a, 0),
                    LineCursor(a, static_cast<LineCursor::Column>(bytes(f.lines.at(0).value).size())),
                    "y",
                    1,
                    RowCursor::None())
                == EDITOR_OK);
    LOKA_VERIFY(f.cursor.state()->get().isNone());
  }
  {
    Fixture f;
    const LineCursor caret(f.lines.at(0).id, 0);
    Observer observer(f);
    const Snapshot snapshot(f);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(caret, caret, 0, 0) == EDITOR_OK);
    snapshot.unchanged(f);
    LOKA_VERIFY(observer.immediate == 0 && observer.cursorNotifications == 0);
    LOKA_VERIFY(
        loka::app::testing::TextEditorAccess::document(f.node).applyReplace(caret, caret, "", 0, RowCursor(2, 1))
        == EDITOR_OK);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(2).id, 1));
    LOKA_VERIFY(!(f.lines.revision().get() != snapshot.revision));
    LOKA_VERIFY(observer.immediate == 0 && observer.cursorNotifications == 1);
    LOKA_VERIFY(
        loka::app::testing::TextEditorAccess::document(f.node).applyReplace(caret, caret, "", 0, RowCursor::None())
        == EDITOR_OK);
    LOKA_VERIFY(f.cursor.state()->get().isNone() && observer.cursorNotifications == 2);
    LOKA_VERIFY(Input::move(*f.context, caret) == EDITOR_OK);
    const Snapshot beforeBackspace(f);
    const unsigned restores = Input::restores(*f.context);
    LOKA_VERIFY(Input::backspace(*f.context) == EDITOR_OK);
    beforeBackspace.unchanged(f);
    LOKA_VERIFY(Input::restores(*f.context) == restores);
  }
  std::printf("[pin] TextEditor range replacement, capacity reuse, post-state caret, empty edit\n");
}
void testTextEditorReplaceRefusals()
{
  Fixture f;
  const LineCursor a(f.lines.at(0).id, 1), c(f.lines.at(2).id, 1);
  Observer observer(f);
  Snapshot snapshot(f);
  const LineCursor invalid[] = {
      LineCursor::None(), LineCursor(a.line, -1), LineCursor(a.line, 5), LineCursor(ItemId(123, 456), 0)};
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
  {
    const EditorResult expected = i == 3 ? EDITOR_STALE_ID : EDITOR_INVALID_CURSOR;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(invalid[i], c, "x", 1) == expected);
    snapshot.unchanged(f);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, invalid[i], "x", 1) == expected);
    snapshot.unchanged(f);
  }
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(c, a, "", 0)
              == EDITOR_INVALID_CURSOR);
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(LineCursor(a.line, 2), a, "", 0)
              == EDITOR_INVALID_CURSOR);
  const RowCursor invalidRows[] = {RowCursor(3, 0), RowCursor(0, -1), RowCursor(0, 3), RowCursor(1, 5)};
  for (unsigned i = 0; i < sizeof(invalidRows) / sizeof(invalidRows[0]); ++i)
  {
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, c, "X\rY", 3, invalidRows[i])
                == EDITOR_INVALID_CURSOR);
    snapshot.unchanged(f);
  }
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, a, "", 0, RowCursor(3, 0))
              == EDITOR_INVALID_CURSOR);
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, a, "", 0, RowCursor(0, -1))
              == EDITOR_INVALID_CURSOR);
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, a, "", 0, RowCursor(0, 5))
              == EDITOR_INVALID_CURSOR);
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, c, 0, 1) == EDITOR_NON_ASCII);
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, c, "\0", 1) == EDITOR_NON_ASCII);
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, c, "\200", 1) == EDITOR_NON_ASCII);
  const std::string oversized(16385, 'x');
  LOKA_VERIFY(
      loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, c, oversized.data(), oversized.size())
      == EDITOR_CAPACITY);
  snapshot.unchanged(f);
  LOKA_VERIFY(observer.immediate == 0 && observer.cursorNotifications == 0);
  // Validation precedes scratch allocation, even when a later layer also refuses.
  const std::string oversizedInvalid(16385, '\0');
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(
                  a, c, oversizedInvalid.data(), oversizedInvalid.size())
              == EDITOR_CAPACITY);
  loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 1);
  LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).applyReplace(a, c, "\200", 1) == EDITOR_NON_ASCII);
  loka::core::testing::allowLokaAllocRaw();
  {
    Fixture tight(3, "abcd", 3);
    const LineCursor first(tight.lines.at(0).id, 0);
    const Snapshot before(tight);
    loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 1);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(tight.node).applyReplace(first, first, "\r", 1)
                == EDITOR_CAPACITY);
    loka::core::testing::allowLokaAllocRaw();
    before.unchanged(tight);
  }
  {
    Fixture full(256, "", 257);
    const LineCursor first(full.lines.at(0).id, 0);
    const Snapshot before(full);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(full.node).applyReplace(first, first, "\r", 1)
                == EDITOR_CAPACITY);
    before.unchanged(full);
  }
  {
    Fixture full(2, std::string(4095, 'x'));
    const LineCursor first(full.lines.at(0).id, 0);
    const Snapshot before(full);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(full.node).applyReplace(first, first, "xx", 2)
                == EDITOR_CAPACITY);
    before.unchanged(full);
  }
  // Removed-byte accounting includes every crossed CR, and subtracts old content.
  {
    Fixture full(3, "");
    const std::string huge(8190, 'x');
    LOKA_VERIFY(full.lines.update(full.lines.at(1).id, String(huge)) == EDIT_OK);
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(full.node).applyReplace(
                    LineCursor(full.lines.at(0).id, 0), LineCursor(full.lines.at(2).id, 0), oversized.data(), 8192)
                == EDITOR_OK);
    LOKA_VERIFY(full.lines.size() == 1 && bytes(full.lines.at(0).value).size() == 8192);
  }
  std::printf("[pin] TextEditor range refusal atomicity and crossed-CR byte accounting\n");
}

namespace
{
  struct RequestObserver
  {
    Fixture &fixture;
    unsigned requests;
    unsigned reports;
    LineCursor repost;
    bool onClear;
    bool onReport;
    RequestObserver(Fixture &f)
        : fixture(f),
          requests(0),
          reports(0),
          repost(),
          onClear(false),
          onReport(false)
    {
      f.request.state()->bind(&requestChanged, this, false);
      f.cursor.state()->bind(&factChanged, this, false);
    }
    ~RequestObserver()
    {
      this->fixture.request.state()->unbind(&requestChanged, this);
      this->fixture.cursor.state()->unbind(&factChanged, this);
    }
    static void requestChanged(void *data)
    {
      RequestObserver &self = *static_cast<RequestObserver *>(data);
      ++self.requests;
      LOKA_VERIFY(self.requests < 20);
      if (self.onClear && self.fixture.request.get().isNone())
      {
        self.onClear = false;
        self.fixture.request.set(self.repost);
      }
    }
    static void factChanged(void *data)
    {
      RequestObserver &self = *static_cast<RequestObserver *>(data);
      ++self.reports;
      // A clear must precede the report, even when publication re-enters props sync.
      LOKA_VERIFY(self.fixture.request.get().isNone());
      if (self.onReport)
      {
        self.onReport = false;
        self.fixture.request.set(self.repost);
        self.fixture.context->onPropsApplied();
        LOKA_VERIFY(self.fixture.request.get() == self.repost);
      }
    }
  };
  /** Reposts on every report until its test budget is exhausted (-1 never stops). */
  struct RepostingSubscriber
  {
    Fixture &fixture;
    int remaining;
    unsigned delivered;
    RepostingSubscriber(Fixture &f, int count)
        : fixture(f),
          remaining(count),
          delivered(0)
    {
      this->fixture.cursor.state()->bind(&changed, this, false);
    }
    ~RepostingSubscriber()
    {
      this->fixture.cursor.state()->unbind(&changed, this);
    }
    static void changed(void *data)
    {
      RepostingSubscriber &self = *static_cast<RepostingSubscriber *>(data);
      ++self.delivered;
      // Fail immediately on a third take, including in the unbounded mutation.
      LOKA_VERIFY(self.delivered <= 2);
      if (self.remaining != 0)
      {
        if (self.remaining > 0)
          --self.remaining;
        LineCursor next = self.fixture.cursor.state()->get();
        next.column = next.column == 0 ? 1 : 0;
        self.fixture.request.set(next);
        self.fixture.context->onPropsApplied();
        LOKA_VERIFY(self.fixture.request.get() == next);
      }
    }
  };
  struct PublicationCount
  {
    unsigned count;
    PublicationCount()
        : count(0)
    {
    }
    static void changed(void *data)
    {
      ++static_cast<PublicationCount *>(data)->count;
    }
  };
  void applyEditorProps(Fixture &f, const TextEditorProps &props)
  {
    TextEditorDefinition definition(props);
    LOKA_VERIFY(definition.applyPropsToNode(&f.node));
  }
} // namespace
void testTextEditorRequestTake()
{
  Fixture f;
  LOKA_VERIFY(f.request.get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 2));
  RequestObserver observer(f);
  const LineCursor requested(f.lines.at(1).id, 99);
  f.request.set(requested);
  f.context->onPropsApplied();
  LOKA_VERIFY(f.request.get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == LineCursor(requested.line, 4));
  LOKA_VERIFY(observer.requests == 2 && observer.reports == 1);
  f.context->onPropsApplied();
  LOKA_VERIFY(observer.requests == 2 && observer.reports == 1);
  f.request.set(requested);
  f.context->onPropsApplied();
  // The equal request is taken again; an unchanged fact need not republish.
  LOKA_VERIFY(observer.requests == 4 && observer.reports == 1);
  LOKA_VERIFY(f.request.get().isNone());
}
void testTextEditorRequestIdle()
{
  Fixture f;
  RequestObserver observer(f);
  // Initialization is complete before observing idle deliveries.
  f.request.set(LineCursor::None());
  observer.requests = 0;
  PublicationCount settled;
  f.tracker.setInvalidateCallback(&PublicationCount::changed, &settled);
  // Positive control: an equal set is silent immediately, but still dirties
  // the request and invalidates its owner. Idle must not do that.
  f.request.set(LineCursor::None());
  LOKA_VERIFY(settled.count == 1);
  settled.count = 0;
  for (int i = 0; i < 3; ++i)
    f.context->onPropsApplied();
  LOKA_VERIFY(observer.requests == 0 && observer.reports == 0);
  LOKA_VERIFY(settled.count == 0);
  f.tracker.setInvalidateCallback(0, 0);
}
void testTextEditorRequestReposts()
{
  for (int duringClear = 0; duringClear < 2; ++duringClear)
  {
    Fixture f;
    RequestObserver observer(f);
    const LineCursor first(f.lines.at(1).id, 1), second(f.lines.at(2).id, 3);
    observer.repost = second;
    observer.onReport = !duringClear;
    // The report pin above requires an empty slot. For a clear-time repost,
    // observe only the request stream until both queued requests are delivered.
    if (duringClear)
    {
      f.cursor.state()->unbind(&RequestObserver::factChanged, &observer);
      observer.onClear = true;
    }
    f.request.set(first);
    f.context->onPropsApplied();
    LOKA_VERIFY(f.request.get().isNone());
    LOKA_VERIFY(f.cursor.state()->get() == second);
    LOKA_VERIFY(observer.requests == 4);
    if (!duringClear)
      LOKA_VERIFY(observer.reports == 2);
  }
  for (int endless = 0; endless < 2; ++endless)
  {
    Fixture f;
    RepostingSubscriber subscriber(f, endless ? -1 : 5);
    PublicationCount invalidations;
    f.tracker.setInvalidateCallback(&PublicationCount::changed, &invalidations);
    f.request.set(LineCursor(f.lines.at(0).id, 0));
    for (int delivery = 0; delivery < 3; ++delivery)
    {
      subscriber.delivered = 0;
      invalidations.count = 0;
      f.context->onPropsApplied();
      LOKA_VERIFY(subscriber.delivered == 2);
      LOKA_VERIFY(invalidations.count > 0);
      LOKA_VERIFY(f.request.get().isNone() == (!endless && delivery == 2));
    }
    f.tracker.setInvalidateCallback(0, 0);
  }

}
void testTextEditorRequestRefusal()
{
  Fixture f;
  const LineCursor before = f.cursor.state()->get();
  RequestObserver observer(f);
  f.request.set(LineCursor(ItemId(123, 456), 1));
  f.context->onPropsApplied();
  LOKA_VERIFY(f.request.get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == before && observer.reports == 0);
  f.lines.detach();
  f.request.set(LineCursor(before.line, 1));
  f.context->onPropsApplied();
  LOKA_VERIFY(f.request.get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == before && observer.reports == 0);
}
void testTextEditorRequestBinding()
{
  Fixture f;
  ObservableList<String> other;
  LOKA_VERIFY(other.attach(&f.tracker, 3) == ATTACH_OK);
  LOKA_VERIFY(other.insert(0, String("B")) == EDIT_OK);
  LOKA_VERIFY(other.at(0).id == f.lines.at(0).id);
  const LineCursor before = f.cursor.state()->get();
  RequestObserver observer(f);
  f.request.set(LineCursor(other.at(0).id, 0));
  applyEditorProps(f, TextEditorProps(other, f.cursor).moveCaretTo(f.request));
  LOKA_VERIFY(f.request.get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == before && observer.reports == 0);
  // Restore the borrow before the stack-local list is destroyed.
  applyEditorProps(f, TextEditorProps(f.lines, f.cursor).moveCaretTo(f.request));
}
void testTextEditorRequestCancellationRepost()
{
  for (int stale = 0; stale < 2; ++stale)
  {
    Fixture f;
    ObservableList<String> other;
    LOKA_VERIFY(other.attach(&f.tracker, 3) == ATTACH_OK);
    LOKA_VERIFY(other.insert(0, String("B")) == EDIT_OK);
    LOKA_VERIFY(other.at(0).id == f.lines.at(0).id);
    const LineCursor before = f.cursor.state()->get();
    RequestObserver observer(f);
    observer.onClear = true;
    observer.repost = LineCursor(stale ? f.lines.at(2).id : other.at(0).id, 1);
    f.request.set(LineCursor(f.lines.at(0).id, 0));
    applyEditorProps(f, TextEditorProps(other, f.cursor).moveCaretTo(f.request));
    LOKA_VERIFY(f.request.get().isNone());
    LOKA_VERIFY(f.cursor.state()->get() == (stale ? before : observer.repost));
    LOKA_VERIFY(observer.requests == 4);
    LOKA_VERIFY(observer.reports == (stale ? 0u : 1u));
    applyEditorProps(f, TextEditorProps(f.lines, f.cursor).moveCaretTo(f.request));
  }
}
void testTextEditorRequestSeatReplacement()
{
  Fixture f;
  Request<LineCursor> &next = f.otherRequest;
  RequestObserver observer(f);
  observer.onClear = true;
  observer.repost = LineCursor(f.lines.at(2).id, 3);
  const LineCursor fresh(f.lines.at(1).id, 1);
  f.request.set(LineCursor(f.lines.at(0).id, 0));
  next.set(fresh);
  // Reports now use the new slot; the old slot's residual intentionally remains.
  f.cursor.state()->unbind(&RequestObserver::factChanged, &observer);
  applyEditorProps(f, TextEditorProps(f.lines, f.cursor).moveCaretTo(next));
  LOKA_VERIFY(f.request.get() == observer.repost);
  LOKA_VERIFY(next.get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == fresh);
  f.context->onPropsApplied();
  LOKA_VERIFY(f.request.get() == observer.repost && f.cursor.state()->get() == fresh);
  applyEditorProps(f, TextEditorProps(f.lines, f.cursor));
}
void testTextEditorRequestDetach()
{
  Fixture f;
  const LineCursor before = f.cursor.state()->get();
  RequestObserver observer(f);
  f.request.set(LineCursor(f.lines.at(1).id, 1));
  NotifySubtreeNodeDetached(&f.node);
  LOKA_VERIFY(f.request.get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == before && observer.reports == 0);
  NotifySubtreeNodeAttached(&f.node);
  f.context->onPropsApplied();
  LOKA_VERIFY(f.request.get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == before && observer.reports == 0);
  f.request.set(LineCursor(f.lines.at(1).id, 1));
  LifecycleFactTestAccess::MarkSubtreeRetired(&f.node);
  LOKA_VERIFY(f.request.get().isNone());
}

void testTextEditorRequestCompletion()
{
  for (int delivery = 0; delivery < 5; ++delivery)
  {
    Fixture f;
    const LineCursor wanted(f.lines.at(1).id, 1);
    if (delivery == 4)
    {
      f.lines.detach();
      f.context->onPropsApplied();
    }
    const LineCursor before = f.cursor.state()->get();
    f.request.set(wanted);
    switch (delivery)
    {
    case 0:
      f.context->readLifecycleFactOnAttach();
      break;
    case 1:
      f.context->onPropsApplied();
      break;
    case 2:
      LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_OK);
      break;
    case 3:
      LOKA_VERIFY(Input::paste(*f.context, "\200") == EDITOR_NON_ASCII);
      break;
    case 4:
      LOKA_VERIFY(Input::type(*f.context, 'x') == EDITOR_UNAVAILABLE);
      break;
    }
    LOKA_VERIFY(f.request.get().isNone());
    LOKA_VERIFY(f.cursor.state()->get() == (delivery == 4 ? before : wanted));
  }
}
void testTextEditorReportedStorage()
{
  class RefusingOwner : public HeadlessStateOwner
  {
  public:
    virtual IStateOwner *stateStorageOwner()
    {
      return 0;
    }
    virtual void noteStateAllocationFailure() {}
  } owner;
  Reported<LineCursor> refused;
  StateBatchBase::CreateImmediateState(&owner, refused, LineCursor::None());
  LOKA_VERIFY(!refused.isValid() && !refused.state());
  loka::app::testing::TextEditorStateOwner storage;
  NullScenePlatformController platform;
  ObservableList<String> lines;
  LOKA_VERIFY(lines.attach(&storage.tracker, 3) == ATTACH_OK);
  LOKA_VERIFY(lines.insert(0, String("abc")) == EDIT_OK);
  const LineCursor wanted(lines.at(0).id, 1);
  {
    TextEditorNode unavailable(TextEditorProps(lines, refused).moveCaretTo(storage.request));
    platform.projectLayoutForTesting(&unavailable, LayoutState());
    NullTextEditorContext &context = *static_cast<NullTextEditorContext *>(unavailable.getContext());
    storage.request.set(wanted);
    context.readLifecycleFactOnAttach();
    LOKA_VERIFY(storage.request.get().isNone());
    LOKA_VERIFY(Input::status(context) == EDITOR_UNAVAILABLE);
  }
  LOKA_VERIFY(storage.cursor.isValid() && storage.cursor.state()->get().isNone());
  {
    loka::app::testing::TextEditorStateOwner foreign;
    TextEditorNode mismatched(TextEditorProps(lines, storage.cursor).moveCaretTo(foreign.request));
    platform.projectLayoutForTesting(&mismatched, LayoutState());
    NullTextEditorContext &context = *static_cast<NullTextEditorContext *>(mismatched.getContext());
    foreign.request.set(wanted);
    context.readLifecycleFactOnAttach();
    LOKA_VERIFY(foreign.request.get().isNone());
    LOKA_VERIFY(storage.cursor.state()->get().isNone());
  }
  PublicationCount publications;
  storage.request.state()->bind(&PublicationCount::changed, &publications, false);
  {
    TextEditorNode direct(TextEditorProps(lines, storage.cursor).moveCaretTo(storage.request));
    storage.request.set(wanted);
    publications.count = 0;
    // A never-attached node never borrowed the request slot.
  }
  LOKA_VERIFY(storage.request.get() == wanted && publications.count == 0);
  {
    TextEditorNode retired(TextEditorProps(lines, storage.cursor).moveCaretTo(storage.request));
    LOKA_VERIFY(retired.lifecycleFact() == NODE_FACT_ATTACHED);
    storage.request.set(wanted);
    LifecycleFactTestAccess::MarkSubtreeRetired(&retired);
    LOKA_VERIFY(storage.request.get().isNone());
    publications.count = 0;
  }
  LOKA_VERIFY(publications.count == 0);
  storage.request.state()->unbind(&PublicationCount::changed, &publications);
}

namespace
{
  typedef loka::app::testing::SettleTrace<LineCursor> Trace;
  typedef Reply<LineCursor> CaretReply;
  struct SettlementSubscriber
  {
    Fixture &f;
    enum Action
    {
      COUNT,
      REPOST,
      REPOST_ONLY,
      RETIRE_CLEAR,
      RETIRE_FACT,
      RETIRE_REPLY,
      REBIND_CLEAR,
      REBIND_FACT
    } action;
    unsigned replies;
    LineCursor repost;
    SettlementSubscriber(Fixture &fixture, Action value)
        : f(fixture),
          action(value),
          replies(0),
          repost(fixture.lines.at(1).id, 1)
    {
      f.request.state()->bind(&cleared, this, false);
      f.cursor.state()->bind(&reported, this, false);
      f.request.reply().state()->bind(&replied, this, false);
    }
    ~SettlementSubscriber()
    {
      f.request.state()->unbind(&cleared, this);
      f.cursor.state()->unbind(&reported, this);
      f.request.reply().state()->unbind(&replied, this);
    }
    static void retire(Fixture &f)
    {
      LifecycleFactTestAccess::MarkSubtreeRetired(&f.node);
      f.node.setContext(0);
    }
    static void cleared(void *data)
    {
      SettlementSubscriber &s = *static_cast<SettlementSubscriber *>(data);
      if (!s.f.request.get().isNone())
        return;
      if (s.action == RETIRE_CLEAR)
        retire(s.f);
      if (s.action == REBIND_CLEAR)
      {
        s.action = COUNT;
        applyEditorProps(s.f, TextEditorProps(s.f.lines, s.f.cursor).moveCaretTo(s.f.otherRequest));
      }
    }
    static void reported(void *data)
    {
      SettlementSubscriber &s = *static_cast<SettlementSubscriber *>(data);
      if (s.action == RETIRE_FACT)
        retire(s.f);
      if (s.action == REBIND_FACT)
      {
        s.action = COUNT;
        applyEditorProps(s.f, TextEditorProps(s.f.lines, s.f.cursor).moveCaretTo(s.f.otherRequest));
      }
    }
    static void replied(void *data)
    {
      SettlementSubscriber &s = *static_cast<SettlementSubscriber *>(data);
      ++s.replies;
      LOKA_VERIFY(s.replies <= 3);
      if (s.action == RETIRE_REPLY)
        retire(s.f);
      if (s.action == REPOST || s.action == REPOST_ONLY)
      {
        s.f.request.set(s.repost);
        if (s.action == REPOST)
          s.f.context->onPropsApplied();
        LOKA_VERIFY(s.f.request.get() == s.repost);
      }
    }
  };
  /** Controlled rail for common order that Null's synchronous projection cannot produce:
      deferred repair, admission closing after take one, and seam refusal after apply. */
  class SettlementProbe : public RailOperation<LineCursor>
  {
  public:
    enum Mode
    {
      OPEN,
      CLOSE_AFTER_FIRST,
      DEFER,
      FAIL_ARM,
      FAIL_AFTER_TAKES,
      REFUSE_REPORT,
      REFUSE_VALIDATE,
      WRITE,
      WRITE_AND_RESTORE
    } mode;
    unsigned admitted, applied, finished, epilogues, repaints, scheduled;
    RequestApplication<LineCursor> completedApplication;
    explicit SettlementProbe(Mode m)
        : mode(m),
          admitted(0),
          applied(0),
          finished(0),
          epilogues(0),
          repaints(0),
          scheduled(0),
          completedApplication(LineCursor::None(), EDITOR_UNAVAILABLE)
    {
    }
    virtual Admission admit(Node &base, RequestBinding<LineCursor> &binding)
    {
      ++this->admitted;
      binding = static_cast<TextEditorNode &>(base).props.moveCaretTo_;
      if (this->mode == FAIL_ARM || this->mode == DEFER || (this->mode == CLOSE_AFTER_FIRST && this->finished))
        return ADMISSION_DEFERRED;
      return binding.state()->get().isNone() ? ADMISSION_EMPTY : ADMISSION_TAKE;
    }
    virtual EditorResult resolve(Node &, const RequestBinding<LineCursor> &)
    {
      LOKA_VERIFY(this->mode != FAIL_ARM);
      return EDITOR_OK;
    }
    virtual EditorResult validate(Node &, const LineCursor &)
    {
      LOKA_VERIFY(this->mode != FAIL_ARM);
      return this->mode == REFUSE_VALIDATE ? EDITOR_STALE_ID : EDITOR_OK;
    }
    virtual RequestApplication<LineCursor> apply(Node &, const LineCursor &value)
    {
      ++this->applied;
      return RequestApplication<LineCursor>(
          value, EDITOR_OK, (this->mode == WRITE || this->mode == WRITE_AND_RESTORE) ? REPAINT : FOLLOW_NONE);
    }
    virtual EditorResult report(Node &base, const LineCursor &value)
    {
      LOKA_VERIFY(this->mode != FAIL_ARM);
      if (this->mode == REFUSE_REPORT)
        return EDITOR_REENTRANT;
      return loka::app::testing::TextEditorAccess::document(static_cast<TextEditorNode &>(base)).moveCaret(value);
    }
    virtual bool current(Node &base, const RequestBinding<LineCursor> &binding)
    {
      return static_cast<TextEditorNode &>(base).props.moveCaretTo_.same(binding);
    }
    virtual FollowUp finishTake(Node &, const CaretReply &, const RequestApplication<LineCursor> &application)
    {
      this->completedApplication = application;
      ++this->finished;
      return this->mode == WRITE_AND_RESTORE ? SCHEDULE_RESTORE : this->mode == WRITE ? REPAINT : FOLLOW_NONE;
    }
    virtual FollowUpResult finishSettle(Node &, const FollowUps &follow)
    {
      ++this->epilogues;
      if (follow.contains(REPAINT))
        ++this->repaints;
      if (this->mode == FAIL_ARM || this->mode == FAIL_AFTER_TAKES)
        return FOLLOW_UP_FAILED;
      if (this->mode == DEFER || follow.contains(SCHEDULE_RESTORE))
      {
        ++this->scheduled;
        return FOLLOW_UP_ARMED;
      }
      return FOLLOW_UP_NONE;
    }
    virtual LineCursor fact(Node &base) const
    {
      return static_cast<TextEditorNode &>(base).props.cursorState()->get();
    }
    FollowUpResult run(Fixture &f)
    {
      return RequestSettlement<LineCursor>::settle(&f.node, f.context, *this, SETTLE_DEFERRED, this->fact(f.node));
    }
  };
} // namespace
void testTextEditorSettlementReplies()
{
  loka::app::testing::TextEditorStateOwner owner;
  LOKA_VERIFY(owner.request.reply().state()->get().kind() == CaretReply::NO_REPLY);
  Fixture f;
  SettlementSubscriber s(f, SettlementSubscriber::COUNT);
  const LineCursor wanted(f.lines.at(1).id, 99), applied(wanted.line, 4);
  f.request.set(LineCursor(f.lines.at(2).id, 1));
  f.request.set(wanted);
  f.context->onPropsApplied();
  CaretReply reply = f.request.reply().state()->get();
  LOKA_VERIFY(s.replies == 1);
  LOKA_VERIFY(reply.kind() == CaretReply::CLAMPED && reply.requested() == wanted && reply.applied() == applied);
  f.request.set(wanted);
  f.context->onPropsApplied();
  LOKA_VERIFY(s.replies == 2);
  s.replies = 0;
  const LineCursor stale(ItemId(999, 999), 0);
  f.request.set(stale);
  f.context->onPropsApplied();
  reply = f.request.reply().state()->get();
  LOKA_VERIFY(reply.kind() == CaretReply::REFUSED && reply.reason() == EDITOR_STALE_ID && reply.requested() == stale);
  LOKA_VERIFY(f.cursor.state()->get() == applied);
  s.action = SettlementSubscriber::REPOST;
  s.replies = 0;
  f.request.set(s.repost);
  f.context->onPropsApplied();
  LOKA_VERIFY(s.replies == 2 && f.request.get() == s.repost);
  LOKA_VERIFY(f.request.reply().state()->get().kind() == CaretReply::GRANTED);
  std::printf("[size] Request=%lu RequestWithReply=%lu Reply=%lu\n",
              static_cast<unsigned long>(sizeof(Request<LineCursor>)),
              static_cast<unsigned long>(sizeof(RequestWithReply<LineCursor>)),
              static_cast<unsigned long>(sizeof(CaretReply)));
}
void testTextEditorSettlementLifetime()
{
  for (int when = 0; when != 3; ++when)
  {
    Fixture f;
    SettlementSubscriber s(f,
                           when == 0   ? SettlementSubscriber::RETIRE_CLEAR
                           : when == 1 ? SettlementSubscriber::RETIRE_FACT
                                       : SettlementSubscriber::RETIRE_REPLY);
    Trace::instance().clear();
    f.request.set(s.repost);
    f.context->onPropsApplied();
    LOKA_VERIFY(f.node.lifecycleFact() == NODE_FACT_RETIRED && f.node.getContext() == 0);
    LOKA_VERIFY(s.replies == (when == 2 ? 1u : 0u));
    LOKA_VERIFY(Trace::instance().size() == 0);
  }
}
void testTextEditorSettlementBinding()
{
  for (int afterReport = 0; afterReport != 2; ++afterReport)
  {
    Fixture f;
    SettlementSubscriber s(f, afterReport ? SettlementSubscriber::REBIND_FACT : SettlementSubscriber::REBIND_CLEAR);
    const LineCursor wanted(f.lines.at(1).id, 1), next(f.lines.at(2).id, 2);
    f.request.set(wanted);
    f.context->onPropsApplied();
    LOKA_VERIFY(s.replies == 0);
    f.otherRequest.set(next);
    f.context->onPropsApplied();
    LOKA_VERIFY(f.otherRequest.get().isNone());
    LOKA_VERIFY(f.cursor.state()->get() == next);
  }
}
void testTextEditorSettlementAdmission()
{
  Fixture f;
  SettlementSubscriber s(f, SettlementSubscriber::REPOST_ONLY);
  SettlementProbe probe(SettlementProbe::CLOSE_AFTER_FIRST);
  f.request.set(s.repost);
  probe.run(f);
  LOKA_VERIFY(probe.admitted == 2 && probe.finished == 1);
  LOKA_VERIFY(s.replies == 1 && f.request.get() == s.repost);
  s.action = SettlementSubscriber::COUNT;
  SettlementProbe deferred(SettlementProbe::DEFER);
  Trace::instance().clear();
  deferred.run(f);
  LOKA_VERIFY(deferred.applied == 0 && deferred.epilogues == 1 && deferred.scheduled == 1);
  LOKA_VERIFY(f.request.get() == s.repost && Trace::instance().size() == 0);
}
void testTextEditorSettlementSeam()
{
  {
    Fixture f(1);
    const String text = String::FromPlatform(Managed<loka::platform::String>::Wrap(
        new loka::app::testing::TextEditorReportRefusal(f.request)));
    LOKA_VERIFY(f.lines.update(f.lines.at(0).id, text) == EDIT_OK);
    f.context->onPropsApplied();
    const LineCursor before = f.cursor.state()->get();
    f.request.set(LineCursor(before.line, 4));
    f.context->onPropsApplied();
    const CaretReply reply = f.request.reply().state()->get();
    LOKA_VERIFY(reply.kind() == CaretReply::REFUSED && reply.reason() == EDITOR_ALLOCATION);
    LOKA_VERIFY(f.cursor.state()->get() == before);
    LOKA_VERIFY(Input::caret(*f.context) == before);
    LOKA_VERIFY(Input::buffer(*f.context) == "abcd");
  }

  {
    Fixture failed;
    const LineCursor before = failed.cursor.state()->get();
    const LineCursor wanted(failed.lines.at(1).id, 1);
    failed.request.set(wanted);
    Trace::instance().clear();
    SettlementProbe arm(SettlementProbe::FAIL_ARM);
    const FollowUpResult result = arm.run(failed);
    LOKA_VERIFY(result == FOLLOW_UP_FAILED);
    LOKA_VERIFY(failed.request.get().isNone());
    const CaretReply refused = failed.request.reply().state()->get();
    LOKA_VERIFY(refused.kind() == CaretReply::REFUSED && refused.reason() == EDITOR_UNAVAILABLE);
    LOKA_VERIFY(refused.requested() == wanted && failed.cursor.state()->get() == before);
    LOKA_VERIFY(arm.admitted == 2 && arm.applied == 0 && arm.finished == 0 && arm.epilogues == 1);
    LOKA_VERIFY(Trace::instance().size() == 1 && Trace::instance().at(0).count == 1);
    LOKA_VERIFY(Trace::instance().at(0).takes[0].kind() == CaretReply::REFUSED
                && Trace::instance().at(0).seam[0] == EDITOR_UNAVAILABLE);
    LOKA_VERIFY(Trace::instance().at(0).before == before && Trace::instance().at(0).after == before);
  }
  {
    Fixture failed;
    SettlementSubscriber subscriber(failed, SettlementSubscriber::REPOST_ONLY);
    failed.request.set(subscriber.repost);
    Trace::instance().clear();
    SettlementProbe arm(SettlementProbe::FAIL_AFTER_TAKES);
    arm.run(failed);
    LOKA_VERIFY(arm.admitted == 2 && arm.applied == 2 && arm.finished == 2 && arm.epilogues == 1);
    LOKA_VERIFY(subscriber.replies == 3 && failed.request.get() == subscriber.repost);
    LOKA_VERIFY(failed.cursor.state()->get() == subscriber.repost);
    LOKA_VERIFY(Trace::instance().size() == 1 && Trace::instance().at(0).count == 3);
    LOKA_VERIFY(Trace::instance().at(0).takes[2].kind() == CaretReply::REFUSED
                && Trace::instance().at(0).takes[2].reason() == EDITOR_UNAVAILABLE);
  }
  for (int when = 0; when != 3; ++when)
  {
    Fixture failed;
    const CaretReply previousReply = failed.request.reply().state()->get();
    SettlementSubscriber subscriber(failed,
                                    when == 0   ? SettlementSubscriber::RETIRE_CLEAR
                                    : when == 1 ? SettlementSubscriber::RETIRE_REPLY
                                                : SettlementSubscriber::REBIND_CLEAR);
    failed.request.set(subscriber.repost);
    Trace::instance().clear();
    SettlementProbe arm(SettlementProbe::FAIL_ARM);
    const FollowUpResult result = arm.run(failed);
    LOKA_VERIFY(result == (when == 2 ? FOLLOW_UP_FAILED : FOLLOW_UP_NONE));
    LOKA_VERIFY(subscriber.replies == (when == 1 ? 1u : 0u));
    LOKA_VERIFY(arm.applied == 0 && arm.finished == 0 && arm.epilogues == 1);
    if (when != 2)
      LOKA_VERIFY(Trace::instance().size() == 0);
    else
      LOKA_VERIFY(!(failed.request.reply().state()->get() != previousReply));
  }
  {
    Fixture f;
    const LineCursor before = f.cursor.state()->get();
    f.request.set(LineCursor(f.lines.at(1).id, 1));
    SettlementProbe probe(SettlementProbe::REFUSE_VALIDATE);
    probe.run(f);
    LOKA_VERIFY(probe.applied == 0 && probe.finished == 1);
    LOKA_VERIFY(probe.completedApplication.result() == EDITOR_STALE_ID);
    LOKA_VERIFY(f.request.reply().state()->get().kind() == CaretReply::REFUSED);
    LOKA_VERIFY(f.cursor.state()->get() == before);
  }
  Fixture f;
  const LineCursor before = f.cursor.state()->get();
  f.request.set(LineCursor(f.lines.at(1).id, 1));
  SettlementProbe probe(SettlementProbe::REFUSE_REPORT);
  probe.run(f);
  const CaretReply reply = f.request.reply().state()->get();
  LOKA_VERIFY(probe.applied == 1 && probe.finished == 1);
  LOKA_VERIFY(probe.completedApplication.result() == EDITOR_OK);
  LOKA_VERIFY(probe.completedApplication.value() == LineCursor(f.lines.at(1).id, 1));
  LOKA_VERIFY(reply.kind() == CaretReply::REFUSED && reply.reason() == EDITOR_REENTRANT);
  LOKA_VERIFY(f.cursor.state()->get() == before);
}
void testTextEditorSettlementTrace()
{
  Fixture f;
  Trace &trace = Trace::instance();
  trace.clear();
  f.context->onPropsApplied();
  LOKA_VERIFY(trace.size() == 0);
  SettlementProbe empty(SettlementProbe::OPEN);
  empty.run(f);
  LOKA_VERIFY(empty.repaints == 0 && empty.finished == 0 && empty.epilogues == 1 && trace.size() == 0);
  f.request.set(LineCursor(f.lines.at(1).id, 1));
  f.context->onPropsApplied();
  LOKA_VERIFY(trace.size() == 1 && trace.at(0).count == 1 && trace.at(0).stimulus == SETTLE_PROPS);
  LOKA_VERIFY(trace.at(0).takes[0].kind() == CaretReply::GRANTED && trace.at(0).seam[0] == EDITOR_OK);
  LOKA_VERIFY(trace.at(0).before != trace.at(0).after);
  trace.clear();
  LOKA_VERIFY(Input::move(*f.context, LineCursor(f.lines.at(2).id, 2)) == EDITOR_OK);
  LOKA_VERIFY(trace.size() == 1 && trace.at(0).count == 0 && trace.at(0).stimulus == SETTLE_INPUT);
  LOKA_VERIFY(trace.at(0).before != trace.at(0).after);
  f.request.set(LineCursor(f.lines.at(0).id, 1));
  SettlementProbe write(SettlementProbe::WRITE);
  write.run(f);
  LOKA_VERIFY(write.repaints == 1);
  f.request.set(LineCursor(f.lines.at(1).id, 1));
  SettlementProbe both(SettlementProbe::WRITE_AND_RESTORE);
  both.run(f);
  LOKA_VERIFY(both.repaints == 1 && both.scheduled == 1 && both.epilogues == 1);
}

namespace
{
  struct QueueFixture : HeadlessStateOwner
  {
    PushStateTracker &tracker;
    RequestQueue<LineCursor, 4> queue;
    Reported<LineCursor> cursor;
    ObservableList<String> lines;
    NullScenePlatformController platform;
    TextEditorNode node;
    NullTextEditorContext *context;
    QueueFixture()
        : tracker(*HeadlessStateOwner::tracker()->asPushTracker()),
          node(TextEditorProps(lines, cursor)), context(0)
    {
      StateBatchBase::CreateImmediateState(this, this->queue, LineCursor::None());
      StateBatchBase::CreateImmediateState(this, this->cursor, LineCursor::None());
      LOKA_VERIFY(this->lines.attach(&this->tracker, 4) == ATTACH_OK);
      LOKA_VERIFY(this->lines.insert(0, String("abcdef")) == EDIT_OK);
      this->node.props = TextEditorProps(this->lines, this->cursor).moveCaretTo(this->queue);
      this->node.setPropsTypeId(TextEditorProps::staticTypeId());
      LayoutState bounds;
      bounds.width = 200;
      bounds.height = 100;
      this->platform.projectLayoutForTesting(&this->node, bounds);
      this->context = static_cast<NullTextEditorContext *>(this->node.getContext());
      LOKA_VERIFY(this->context);
    }
    LineCursor at(unsigned column) const { return LineCursor(this->lines.at(0).id, column); }
    void apply(const TextEditorProps &props)
    {
      TextEditorDefinition definition(props);
      LOKA_VERIFY(definition.applyPropsToNode(&this->node));
    }
  };
  struct QueueReplies
  {
    RequestQueueBase<LineCursor> &queue;
    std::vector<CaretReply> values;
    explicit QueueReplies(RequestQueueBase<LineCursor> &q) : queue(q)
    {
      this->queue.reply().state()->bind(&changed, this, false);
    }
    ~QueueReplies() { this->queue.reply().state()->unbind(&changed, this); }
    static void changed(void *data)
    {
      QueueReplies &self = *static_cast<QueueReplies *>(data);
      self.values.push_back(self.queue.reply().state()->get());
    }
  };
  struct QueueReentry
  {
    enum Action { POST_NEXT, CANCEL_REPOST, DISCARD_NEXT };
    QueueFixture &fixture;
    Action action;
    unsigned calls;
    QueueReentry(QueueFixture &f, Action a) : fixture(f), action(a), calls(0)
    {
      f.queue.state()->bind(&changed, this, false);
    }
    ~QueueReentry() { this->fixture.queue.state()->unbind(&changed, this); }
    static void changed(void *data)
    {
      QueueReentry &self = *static_cast<QueueReentry *>(data);
      QueueFixture &f = self.fixture;
      if (self.calls)
        return;
      if (self.action == CANCEL_REPOST ? !f.queue.state()->get().isNone()
                                      : f.queue.state()->get() != f.at(2))
        return;
      ++self.calls;
      if (self.action == DISCARD_NEXT)
        f.apply(TextEditorProps(f.lines, f.cursor));
      else
      {
        LOKA_VERIFY(f.queue.post(f.at(4)) == POST_ACCEPTED);
        if (self.action == CANCEL_REPOST)
          LOKA_VERIFY(f.queue.post(f.at(5)) == POST_ACCEPTED);
      }
    }
  };
  struct QueuePropsAccess : TextEditorProps
  {
    static RequestBinding<LineCursor> withoutSource(RequestQueueBase<LineCursor> &queue)
    {
      return RequestBinding<LineCursor>(requestSeat(queue), replySeat(queue));
    }
  };
  class QueueSceneRoot : public BoundaryNodeFor<QueueSceneRoot>
  {
  public:
    ObservableList<String> lines;
    Reported<LineCursor> cursor;
    RequestQueue<LineCursor, 4> queue;
    explicit QueueSceneRoot(const BoundaryPropsFor<QueueSceneRoot> &p) : BoundaryNodeFor<QueueSceneRoot>(p)
    {
      this->declareStates(3).state(this->cursor, LineCursor::None()).state(this->queue, LineCursor::None());
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const { return false; }
    virtual void attachNode(NodeComposition &)
    {
      StateTracker *owner = 0;
      if (this->lines.queryMutationTracker(owner) == EDIT_OK)
        return;
      LOKA_VERIFY(this->lines.attach(this->tracker()->asPushTracker(), 4) == ATTACH_OK);
      LOKA_VERIFY(this->lines.insert(0, String("hello")) == EDIT_OK);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(TextEditor(TextEditorProps(this->lines, this->cursor).moveCaretTo(this->queue)));
    }
  };
}
void testRequestQueueFailedArmDoesNotStrandRemainder()
{
  QueueFixture f;
  QueueReplies replies(f.queue);
  for (unsigned i = 1; i <= 4; ++i)
    LOKA_VERIFY(f.queue.post(f.at(i)) == POST_ACCEPTED);
  Trace::instance().clear();
  SettlementProbe arm(SettlementProbe::FAIL_ARM);
  LOKA_VERIFY(RequestSettlement<LineCursor>::settle(&f.node, f.context, arm, SETTLE_DEFERRED,
                                                   f.cursor.state()->get()) == FOLLOW_UP_FAILED);
  LOKA_VERIFY(f.queue.pending() == 2 && f.queue.state()->get() == f.at(2));
  LOKA_VERIFY(replies.values.size() == 1 && replies.values[0].requested() == f.at(1));
  LOKA_VERIFY(replies.values[0].kind() == CaretReply::REFUSED
              && replies.values[0].reason() == EDITOR_UNAVAILABLE);
  LOKA_VERIFY(Trace::instance().size() == 1 && Trace::instance().at(0).count == 1);
  LOKA_VERIFY(Trace::instance().at(0).takes[0].requested() == f.at(1)
              && Trace::instance().at(0).seam[0] == EDITOR_UNAVAILABLE);
  f.context->onPropsApplied();
  LOKA_VERIFY(replies.values.size() == 3 && f.queue.state()->get() == f.at(4));
  f.context->onPropsApplied();
  LOKA_VERIFY(replies.values.size() == 4 && f.queue.state()->get().isNone() && f.queue.pending() == 0);
  for (unsigned i = 1; i < 4; ++i)
    LOKA_VERIFY(replies.values[i].kind() == CaretReply::GRANTED && replies.values[i].requested() == f.at(i + 1));
  LOKA_VERIFY(Trace::instance().size() == 3 && Trace::instance().at(1).count == 2
              && Trace::instance().at(2).count == 1);
}
void testRequestQueuePublishRepostCannotOvertakeRing()
{
  QueueFixture f;
  QueueReplies replies(f.queue);
  for (unsigned i = 1; i <= 3; ++i)
    LOKA_VERIFY(f.queue.post(f.at(i)) == POST_ACCEPTED);
  QueueReentry reentry(f, QueueReentry::POST_NEXT);
  f.context->onPropsApplied();
  LOKA_VERIFY(reentry.calls == 1 && f.queue.pending() == 1);
  f.context->onPropsApplied();
  LOKA_VERIFY(replies.values.size() == 4 && f.queue.state()->get().isNone());
  for (unsigned i = 0; i < 4; ++i)
    LOKA_VERIFY(replies.values[i].requested() == f.at(i + 1) && replies.values[i].kind() == CaretReply::GRANTED);
}
void testRequestQueueCancellationCannotEraseRepost()
{
  QueueFixture f;
  ObservableList<String> nextLines;
  LOKA_VERIFY(nextLines.attach(&f.tracker, 4) == ATTACH_OK);
  LOKA_VERIFY(nextLines.insert(0, String("abcdef")) == EDIT_OK);
  QueueReplies replies(f.queue);
  for (unsigned i = 1; i <= 3; ++i)
    LOKA_VERIFY(f.queue.post(f.at(i)) == POST_ACCEPTED);
  QueueReentry reentry(f, QueueReentry::CANCEL_REPOST);
  LOKA_VERIFY((NodePropsApplier<TextEditorNode, TextEditorProps>::apply(
      &f.node, TextEditorProps(nextLines, f.cursor).moveCaretTo(f.queue))));
  LOKA_VERIFY(reentry.calls == 1 && f.queue.state()->get() == f.at(4) && f.queue.pending() == 1);
  LOKA_VERIFY(replies.values.empty());
  f.context->onPropsApplied();
  LOKA_VERIFY(replies.values.size() == 2);
  LOKA_VERIFY(replies.values[0].requested() == f.at(4) && replies.values[1].requested() == f.at(5));
  LOKA_VERIFY(f.queue.state()->get().isNone() && f.queue.pending() == 0);
  f.apply(TextEditorProps(f.lines, f.cursor).moveCaretTo(f.queue));
}
void testRequestQueueBindingSourceCannotBeIgnored()
{
  QueueFixture f;
  QueueReplies replies(f.queue);
  LOKA_VERIFY(f.queue.post(f.at(1)) == POST_ACCEPTED);
  LOKA_VERIFY(f.queue.post(f.at(2)) == POST_ACCEPTED);
  TextEditorProps next(f.lines, f.cursor);
  next.moveCaretTo_ = QueuePropsAccess::withoutSource(f.queue);
  LOKA_VERIFY(!next.moveCaretTo_.same(f.node.props.moveCaretTo_));
  LOKA_VERIFY((next < f.node.props) != (f.node.props < next));
  f.apply(next);
  LOKA_VERIFY(f.queue.state()->get().isNone() && f.queue.pending() == 0 && replies.values.empty());
}
void testRequestQueueEqualPostsCannotLoseSceneContinuation()
{
  EditorPresenter platform;
  Scene scene((Boundary<QueueSceneRoot>()));
  scene.mount(&platform);
  QueueSceneRoot *root = static_cast<QueueSceneRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(root);
  LayoutState bounds;
  bounds.width = 200;
  bounds.height = 100;
  platform.projectLayoutForTesting(root, bounds);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  for (unsigned i = 0; scene.hasPendingInvalidation() && i < 12; ++i)
    LOKA_VERIFY(scene.flushInvalidation());
  QueueReplies replies(root->queue);
  const LineCursor wanted(root->lines.at(0).id, 2);
  Trace::instance().clear();
  {
    StateTrackerGuard guard(root->tracker());
    for (unsigned i = 0; i < 3; ++i)
      LOKA_VERIFY(root->queue.post(wanted) == POST_ACCEPTED);
  }
  for (unsigned i = 0; scene.hasPendingInvalidation() && i < 12; ++i)
    LOKA_VERIFY(scene.flushInvalidation());
  LOKA_VERIFY(replies.values.size() == 3 && root->queue.state()->get().isNone());
  for (unsigned i = 0; i < 3; ++i)
    LOKA_VERIFY(replies.values[i].requested() == wanted && replies.values[i].kind() == CaretReply::GRANTED);
  LOKA_VERIFY(Trace::instance().size() == 2 && Trace::instance().at(0).count == 2
              && Trace::instance().at(1).count == 1);
}
void testRequestQueueDirectNextDiscardCannotApplyOldSnapshot()
{
  QueueFixture f;
  QueueReplies replies(f.queue);
  const LineCursor before = f.cursor.state()->get();
  const LineCursor nativeBefore = Input::caret(*f.context);
  for (unsigned i = 1; i <= 3; ++i)
    LOKA_VERIFY(f.queue.post(f.at(i)) == POST_ACCEPTED);
  QueueReentry reentry(f, QueueReentry::DISCARD_NEXT);
  f.context->onPropsApplied();
  LOKA_VERIFY(reentry.calls == 1 && replies.values.empty());
  LOKA_VERIFY(f.queue.pending() == 0 && f.queue.state()->get().isNone());
  LOKA_VERIFY(f.cursor.state()->get() == before);
  LOKA_VERIFY(Input::caret(*f.context) == nativeBefore);
}
void testRequestQueueRefusedPostsCannotMutateEndpoint()
{
  QueueFixture f;
  QueueReplies replies(f.queue);
  const CaretReply before = f.queue.reply().state()->get();
  // NodeState + Reported + ring pointer/counters + four LineCursors:
  // 64 bytes on 32-bit ABIs, 96 bytes on the host's 64-bit ABI.
  LOKA_VERIFY(sizeof(RequestQueue<LineCursor, 4>) <= (sizeof(void *) == 4 ? 64u : 96u));
  for (unsigned i = 0; i < 5; ++i)
    LOKA_VERIFY(f.queue.post(f.at(i)) == POST_ACCEPTED);
  LOKA_VERIFY(f.queue.pending() == 4 && f.queue.state()->get() == f.at(0));
  LOKA_VERIFY(f.queue.post(f.at(5)) == POST_QUEUE_FULL);
  LOKA_VERIFY(f.queue.post(LineCursor::None()) == POST_INVALID);
  LOKA_VERIFY(f.queue.pending() == 4 && f.queue.state()->get() == f.at(0));
  LOKA_VERIFY(!(f.queue.reply().state()->get() != before) && replies.values.empty());
  for (unsigned i = 0; i < 3; ++i)
    f.context->onPropsApplied();
  LOKA_VERIFY(replies.values.size() == 5);
  for (unsigned i = 0; i < 5; ++i)
    LOKA_VERIFY(replies.values[i].requested() == f.at(i));
}
void testRequestQueueDetachCannotReplayRing()
{
  QueueFixture f;
  QueueReplies replies(f.queue);
  LOKA_VERIFY(f.queue.post(f.at(1)) == POST_ACCEPTED);
  LOKA_VERIFY(f.queue.post(f.at(2)) == POST_ACCEPTED);
  NotifySubtreeNodeDetached(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  LOKA_VERIFY(f.queue.state()->get().isNone() && f.queue.pending() == 0);
  NotifySubtreeNodeAttached(&f.node);
  LifecycleFactTestAccess::DeliverFacts(&f.node);
  f.context->readLifecycleFactOnAttach();
  LOKA_VERIFY(f.queue.state()->get().isNone() && f.queue.pending() == 0 && replies.values.empty());
}
