#include "TextEditorTests.hpp"
#include "../win32/src/context/Win32TextEditorDiff.hpp"
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
        ids.push_back(f.lines.at(i).id);
        text.push_back(f.lines.at(i).value);
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
  // Pure Win32 range detection also runs on the Linux host. Selection is not
  // an input: undo at line zero must remain line zero with a distant caret.
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
    const loka::win32::TextEditorLineDiff diff = loka::win32::DiffTextEditorLines(cases[i].before, cases[i].after);
    LOKA_VERIFY(diff.first() == cases[i].first && diff.before() == cases[i].oldCount
                && diff.after() == cases[i].newCount);
  }
  const std::string atCap = std::string(2, 'a') + 'x' + std::string(8189, 'a');
  const loka::win32::TextEditorLineDiff capDiff = loka::win32::DiffTextEditorLines(std::string(8191, 'a'), atCap);
  LOKA_VERIFY(capDiff.first() == 0 && capDiff.before() == 1 && capDiff.after() == 1);
  LOKA_VERIFY(loka::win32::TextEditorLogicalLine(atCap, 0).size() == 8192);

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
