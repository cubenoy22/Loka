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
  struct Fixture
  {
    PushStateTracker tracker;
    ObservableList<String> lines;
    MutableState<LineCursor> cursor;
    NodeState<LineCursor> seat;
    NullScenePlatformController platform;
    TextEditorNode node;
    NullTextEditorContext *context;
    explicit Fixture(unsigned short count = 3, const std::string &text = "abcd", unsigned short capacity = 256)
        : tracker(),
          lines(),
          cursor(),
          seat(&cursor, &tracker),
          platform(),
          node(TextEditorProps(lines, seat)),
          context(0)
    {
      this->tracker.addState(&this->cursor);
      LOKA_VERIFY(this->lines.attach(&this->tracker, capacity) == ATTACH_OK);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(this->lines.insert(i, String(text)) == EDIT_OK);
      if (count)
      {
        StateTrackerGuard guard(&this->tracker);
        this->cursor.set(LineCursor(this->lines.at(0).id, 2));
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
      f.cursor.bind(&cursorChanged, this, false);
    }
    static void cursorChanged(void *data)
    {
      ++static_cast<Observer *>(data)->cursorNotifications;
    }
    ~Observer()
    {
      fixture.cursor.unbind(&cursorChanged, this);
      const_cast<State<ListRevision> &>(fixture.lines.revision()).unbind(&changed, this);
    }
    static void changed(void *data)
    {
      Observer &self = *static_cast<Observer *>(data);
      ++self.immediate;
      self.fixture.tracker.defer(&flushed, &self);
      LOKA_VERIFY(self.fixture.node.document.moveCaret(self.fixture.cursor.get()) == EDITOR_REENTRANT);
      const LineCursor caret(self.fixture.lines.at(0).id, 0);
      LOKA_VERIFY(self.fixture.node.document.applyReplace(caret, caret, "", 0) == EDITOR_REENTRANT);
      LOKA_VERIFY(self.fixture.node.document.applyReplace(caret, caret, "", 0, RowCursor::None()) == EDITOR_REENTRANT);
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
        LOKA_VERIFY(self.fixture.cursor.get() == self.expected);
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
    LOKA_VERIFY(f.node.document.project(projection) == EDITOR_OK);
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

  // Identity pins consume the pure diff through the document's split/join doors.
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
      LOKA_VERIFY(split.node.document.applySplit(
                      split.lines.at(static_cast<unsigned short>(diff.first())).id,
                      static_cast<LineCursor::Column>(TextEditorLogicalLine(after, diff.first()).size()))
                  == EDITOR_OK);
      LOKA_VERIFY(split.lines.size() == count + 1);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(split.lines.at(i <= line ? i : i + 1).id == original.ids[i]);
      const ItemId added = split.lines.at(static_cast<unsigned short>(line + 1)).id;
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(added != original.ids[i]);
      LOKA_VERIFY(bytes(split.lines.at(static_cast<unsigned short>(line)).value) == (column ? "a" : ""));
      LOKA_VERIFY(bytes(split.lines.at(static_cast<unsigned short>(line + 1)).value) == (column ? "" : "a"));
      LOKA_VERIFY(split.cursor.get() == LineCursor(added, 0));
      std::string projected;
      LOKA_VERIFY(split.node.document.project(projected) == EDITOR_OK && projected == after);
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
      LOKA_VERIFY(join.node.document.applyJoin(join.lines.at(static_cast<unsigned short>(diff.first() + 1)).id)
                  == EDITOR_OK);
      LOKA_VERIFY(join.lines.size() == count - 1 && join.lines.at(0).id == original.ids[0]);
      LOKA_VERIFY(join.lines.find(original.ids[1]) < 0);
      if (count == 3)
        LOKA_VERIFY(join.lines.at(1).id == original.ids[2]);
      LOKA_VERIFY(join.cursor.get() == LineCursor(original.ids[0], empty ? 0 : 1));
      std::string projected;
      LOKA_VERIFY(join.node.document.project(projected) == EDITOR_OK && projected == after);
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
  LOKA_VERIFY(f.cursor.get() == LineCursor(f.lines.at(1).id, 0));
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
  LOKA_VERIFY(f.lines.revision().get().structure == initial.structure + 1);
  const unsigned beforeJoin = observer.immediate;
  LOKA_VERIFY(Input::backspace(*f.context) == EDITOR_OK);
  LOKA_VERIFY(observer.immediate == beforeJoin + 1);
  LOKA_VERIFY(f.lines.size() == 3 && f.lines.at(1).id == second && f.lines.at(2).id == third);
  LOKA_VERIFY(bytes(f.lines.at(0).value) == "abxcd" && f.cursor.get() == LineCursor(first, 3));
  LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
  const ListRevision beforeMove = f.lines.revision().get();
  LOKA_VERIFY(Input::move(*f.context, LineCursor(second, 1)) == EDITOR_OK);
  LOKA_VERIFY(!(f.lines.revision().get() != beforeMove));
  LOKA_VERIFY(Input::paste(*f.context, "Q\r\nR\nS") == EDITOR_OK);
  LOKA_VERIFY(bytes(f.lines.at(1).value) == "aQ" && bytes(f.lines.at(2).value) == "R"
              && bytes(f.lines.at(3).value) == "Sbcd");
  LOKA_VERIFY(f.lines.at(4).id == third && f.cursor.get() == LineCursor(f.lines.at(3).id, 1));
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
    LOKA_VERIFY(Input::caret(*f.context) == f.cursor.get() && Input::restores(*f.context) == 1);
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
    PushStateTracker foreignTracker;
    NodeState<LineCursor> foreign(&f.cursor, &foreignTracker);
    f.node.props.cursor(foreign);
    LOKA_VERIFY(f.node.document.applySplit(f.lines.at(0).id, 2) == EDITOR_OWNER_MISMATCH);
    snapshot.unchanged(f);
    LOKA_VERIFY(observer.immediate == 0 && observer.cursorNotifications == 0);
    f.node.props.cursor(f.seat);
    LOKA_VERIFY(f.node.document.applySingleLine(f.lines.at(0).id, String("direct"), LineCursor(f.lines.at(0).id, 6))
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
    NodeState<LineCursor> cursor;
    explicit EditorSceneRoot(const BoundaryPropsFor<EditorSceneRoot> &p)
        : BoundaryNodeFor<EditorSceneRoot>(p)
    {
      this->declareStates(1).state(this->cursor, LineCursor::None());
    }
    virtual void attachNode(NodeComposition &)
    {
      StateTracker *owner = 0;
      if (this->lines.queryMutationTracker(owner) == EDIT_OK)
        return;
      LOKA_VERIFY(this->lines.attach(this->tracker()->asPushTracker(), 258) == ATTACH_OK);
      LOKA_VERIFY(this->lines.insert(0, String("hello")) == EDIT_OK);
      this->cursor.set(LineCursor(this->lines.at(0).id, 1));
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(TextEditor(this->lines, this->cursor));
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
  LOKA_VERIFY(bytes(root->lines.at(0).value) == "hxello" && root->cursor.get().column == 2);
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
    LOKA_VERIFY(f.node.document.applyReplace(LineCursor(a, 1), LineCursor(c, 1), "X\rY", 3) == EDITOR_OK);
    LOKA_VERIFY(f.lines.size() == 2 && f.lines.at(0).id == a);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "aX" && bytes(f.lines.at(1).value) == "Yf");
    const ItemId d = f.lines.at(1).id;
    LOKA_VERIFY(d != a && d != b && d != c && f.lines.find(b) < 0 && f.lines.find(c) < 0);
    LOKA_VERIFY(f.cursor.get() == LineCursor(d, 1));
    LOKA_VERIFY(observer.immediate == 1 && observer.settled == 1 && observer.cursorNotifications == 1);
    LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
    // Grow back to the reserved capacity, then refuse a final size that exceeds it.
    LOKA_VERIFY(f.node.document.applyReplace(LineCursor(a, 1), LineCursor(d, 1), "1\r2\r3", 5) == EDITOR_OK);
    LOKA_VERIFY(f.lines.size() == 3 && bytes(f.lines.at(2).value) == "3f");
    Snapshot snapshot(f);
    LOKA_VERIFY(f.node.document.applyReplace(LineCursor(a, 0), LineCursor(a, 1), "x\ry", 3) == EDITOR_CAPACITY);
    snapshot.unchanged(f);
  }
  // Full capacity with a nonshrinking replacement still needs REMOVE before INSERT.
  {
    Fixture f(3, "abcd", 3);
    const ItemId a = f.lines.at(0).id, c = f.lines.at(2).id;
    LOKA_VERIFY(f.node.document.applyReplace(LineCursor(a, 1), LineCursor(c, 2), "X\r\nY\nZ", 6) == EDITOR_OK);
    LOKA_VERIFY(f.lines.size() == 3 && f.lines.at(0).id == a);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "aX" && bytes(f.lines.at(1).value) == "Y"
                && bytes(f.lines.at(2).value) == "Zcd");
    LOKA_VERIFY(f.cursor.get() == LineCursor(f.lines.at(2).id, 1));
  }
  {
    Fixture f(1);
    const ItemId a = f.lines.at(0).id;
    LOKA_VERIFY(f.node.document.applyReplace(LineCursor(a, 2), LineCursor(a, 2), "\r", 1, RowCursor(1, 1))
                == EDITOR_OK);
    LOKA_VERIFY(bytes(f.lines.at(0).value) == "ab" && bytes(f.lines.at(1).value) == "cd");
    LOKA_VERIFY(f.cursor.get() == LineCursor(f.lines.at(1).id, 1));
    LOKA_VERIFY(f.node.document.applyReplace(LineCursor(a, 0), LineCursor(a, 1), "Z", 1, RowCursor::None())
                == EDITOR_OK);
    LOKA_VERIFY(f.cursor.get() == LineCursor::None());
  }
  // Post-state rows before, inside, and after the replaced range use different lengths.
  for (unsigned short row = 0; row < 4; ++row)
  {
    Fixture f(4, "abcd");
    const ItemId first = f.lines.at(1).id, last = f.lines.at(2).id, tail = f.lines.at(3).id;
    const LineCursor::Column column = row == 1 ? 2 : row == 2 ? 3 : 4;
    LOKA_VERIFY(
        f.node.document.applyReplace(LineCursor(first, 1), LineCursor(last, 2), "X\rY", 3, RowCursor(row, column))
        == EDITOR_OK);
    LOKA_VERIFY(f.cursor.get() == LineCursor(f.lines.at(row).id, column));
    LOKA_VERIFY(f.lines.at(3).id == tail);
  }
  for (int grow = 0; grow < 2; ++grow)
  {
    Fixture f(4, "abcd");
    const ItemId tail = f.lines.at(3).id;
    LOKA_VERIFY(f.node.document.applyReplace(LineCursor(f.lines.at(1).id, 0),
                                             LineCursor(f.lines.at(2).id, 4),
                                             grow ? "x\ry\rz" : "x",
                                             grow ? 5 : 1,
                                             RowCursor(grow ? 4 : 2, 4))
                == EDITOR_OK);
    LOKA_VERIFY(f.cursor.get() == LineCursor(tail, 4));
  }
  // applySingleLine translates the supplied caret identity, including another row.
  {
    Fixture f;
    const ItemId a = f.lines.at(0).id, c = f.lines.at(2).id;
    LOKA_VERIFY(f.node.document.applySingleLine(a, String("x"), LineCursor(c, 4)) == EDITOR_OK);
    LOKA_VERIFY(f.cursor.get() == LineCursor(c, 4));
    LOKA_VERIFY(f.node.document.applySingleLine(a, String("y"), LineCursor::None()) == EDITOR_OK);
    LOKA_VERIFY(f.cursor.get().isNone());
  }
  {
    Fixture f;
    const LineCursor caret(f.lines.at(0).id, 0);
    Observer observer(f);
    const Snapshot snapshot(f);
    LOKA_VERIFY(f.node.document.applyReplace(caret, caret, 0, 0) == EDITOR_OK);
    snapshot.unchanged(f);
    LOKA_VERIFY(observer.immediate == 0 && observer.cursorNotifications == 0);
    LOKA_VERIFY(f.node.document.applyReplace(caret, caret, "", 0, RowCursor(2, 1)) == EDITOR_OK);
    LOKA_VERIFY(f.cursor.get() == LineCursor(f.lines.at(2).id, 1));
    LOKA_VERIFY(!(f.lines.revision().get() != snapshot.revision));
    LOKA_VERIFY(observer.immediate == 0 && observer.cursorNotifications == 1);
    LOKA_VERIFY(f.node.document.applyReplace(caret, caret, "", 0, RowCursor::None()) == EDITOR_OK);
    LOKA_VERIFY(f.cursor.get().isNone() && observer.cursorNotifications == 2);
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
    LOKA_VERIFY(f.node.document.applyReplace(invalid[i], c, "x", 1) == expected);
    snapshot.unchanged(f);
    LOKA_VERIFY(f.node.document.applyReplace(a, invalid[i], "x", 1) == expected);
    snapshot.unchanged(f);
  }
  LOKA_VERIFY(f.node.document.applySingleLine(a.line, String("x"), invalid[3]) == EDITOR_STALE_ID);
  snapshot.unchanged(f);
  LOKA_VERIFY(f.node.document.applyReplace(c, a, "", 0) == EDITOR_INVALID_CURSOR);
  LOKA_VERIFY(f.node.document.applyReplace(LineCursor(a.line, 2), a, "", 0) == EDITOR_INVALID_CURSOR);
  const RowCursor invalidRows[] = {RowCursor(3, 0), RowCursor(0, -1), RowCursor(0, 3), RowCursor(1, 5)};
  for (unsigned i = 0; i < sizeof(invalidRows) / sizeof(invalidRows[0]); ++i)
  {
    LOKA_VERIFY(f.node.document.applyReplace(a, c, "X\rY", 3, invalidRows[i]) == EDITOR_INVALID_CURSOR);
    snapshot.unchanged(f);
  }
  LOKA_VERIFY(f.node.document.applyReplace(a, a, "", 0, RowCursor(3, 0)) == EDITOR_INVALID_CURSOR);
  LOKA_VERIFY(f.node.document.applyReplace(a, a, "", 0, RowCursor(0, -1)) == EDITOR_INVALID_CURSOR);
  LOKA_VERIFY(f.node.document.applyReplace(a, a, "", 0, RowCursor(0, 5)) == EDITOR_INVALID_CURSOR);
  LOKA_VERIFY(f.node.document.applyReplace(a, c, 0, 1) == EDITOR_NON_ASCII);
  LOKA_VERIFY(f.node.document.applyReplace(a, c, "\0", 1) == EDITOR_NON_ASCII);
  LOKA_VERIFY(f.node.document.applyReplace(a, c, "\200", 1) == EDITOR_NON_ASCII);
  const std::string oversized(16385, 'x');
  LOKA_VERIFY(f.node.document.applyReplace(a, c, oversized.data(), oversized.size()) == EDITOR_CAPACITY);
  snapshot.unchanged(f);
  LOKA_VERIFY(observer.immediate == 0 && observer.cursorNotifications == 0);
  // Validation precedes scratch allocation, even when a later layer also refuses.
  const std::string oversizedInvalid(16385, '\0');
  LOKA_VERIFY(f.node.document.applyReplace(a, c, oversizedInvalid.data(), oversizedInvalid.size()) == EDITOR_CAPACITY);
  loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 1);
  LOKA_VERIFY(f.node.document.applyReplace(a, c, "\200", 1) == EDITOR_NON_ASCII);
  loka::core::testing::allowLokaAllocRaw();
  {
    Fixture tight(3, "abcd", 3);
    const LineCursor first(tight.lines.at(0).id, 0);
    const Snapshot before(tight);
    loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 1);
    LOKA_VERIFY(tight.node.document.applyReplace(first, first, "\r", 1) == EDITOR_CAPACITY);
    loka::core::testing::allowLokaAllocRaw();
    before.unchanged(tight);
  }
  {
    Fixture full(256, "", 257);
    const LineCursor first(full.lines.at(0).id, 0);
    const Snapshot before(full);
    LOKA_VERIFY(full.node.document.applyReplace(first, first, "\r", 1) == EDITOR_CAPACITY);
    before.unchanged(full);
  }
  {
    Fixture full(2, std::string(4095, 'x'));
    const LineCursor first(full.lines.at(0).id, 0);
    const Snapshot before(full);
    LOKA_VERIFY(full.node.document.applyReplace(first, first, "xx", 2) == EDITOR_CAPACITY);
    before.unchanged(full);
  }
  // Removed-byte accounting includes every crossed CR, and subtracts old content.
  {
    Fixture full(3, "");
    const std::string huge(8190, 'x');
    LOKA_VERIFY(full.lines.update(full.lines.at(1).id, String(huge)) == EDIT_OK);
    LOKA_VERIFY(full.node.document.applyReplace(
                    LineCursor(full.lines.at(0).id, 0), LineCursor(full.lines.at(2).id, 0), oversized.data(), 8192)
                == EDITOR_OK);
    LOKA_VERIFY(full.lines.size() == 1 && bytes(full.lines.at(0).value).size() == 8192);
  }
  std::printf("[pin] TextEditor range refusal atomicity and crossed-CR byte accounting\n");
}
