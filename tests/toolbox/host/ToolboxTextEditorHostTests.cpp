#include "context/ToolboxTextEditorContext.hpp"
#include "ToolboxBuiltInSupport.hpp"
#include "context/ToolboxPaintSupport.hpp"
#include "support/TestVerify.hpp"
#include "support/TextEditorContractSnapshot.hpp"
#include "support/LokaAllocFailure.hpp"
#include <cstdio>
#include "toolbox/ToolboxTextEditorAccess.hpp"
using namespace loka::app;
using namespace loka::app::scene;
using namespace loka::core;
using loka::testing::ToolboxTextEditorAccess;
namespace
{
  struct Fixture
  {
    PushStateTracker tracker;
    ObservableList<String> lines;
    MutableState<LineCursor> cursor;
    NodeState<LineCursor> seat;
    ToolboxWindow window;
    ToolboxScenePlatformController controller;
    TextEditorNode node;
    ToolboxTextEditorContext *context;
    Fixture(unsigned short count = 3, const std::string &text = "abcd", unsigned short capacity = 256)
        : seat(&cursor, &tracker),
          controller(&window),
          node(TextEditorProps(lines, seat)),
          context(0)
    {
      tracker.addState(&cursor);
      LOKA_VERIFY(lines.attach(&tracker, capacity) == ATTACH_OK);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(lines.insert(i, String(text)) == EDIT_OK);
      {
        StateTrackerGuard guard(&tracker);
        cursor.set(LineCursor(lines.at(0).id, std::min(2, static_cast<int>(text.size()))));
      }
      LOKA_VERIFY(RegisterToolboxBuiltInSupport(controller));
      LayoutState state;
      state.x = 10;
      state.y = 20;
      state.width = 200;
      state.height = 80;
      IPlatformNodeHandler *handler = controller.nodeHandlerRegistry_.find(&node);
      LOKA_VERIFY(handler);
      context = static_cast<ToolboxTextEditorContext *>(handler->ensureContext(&node, &controller, state));
      LOKA_VERIFY(context);
      context->layout(&controller, state);
      LOKA_VERIFY(!ToolboxTextEditorAccess::te(*context));
      context->render(&controller);
    }
    ~Fixture()
    {
      context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_RETIRED);
      controller.flushTE();
    }
    TEHandle te()
    {
      return ToolboxTextEditorAccess::te(*context);
    }
    std::string native()
    {
      return (**te()).text;
    }
    unsigned restores()
    {
      return ToolboxTextEditorAccess::restores(*context);
    }
  };
  struct Snapshot : loka::testing::TextEditorContractSnapshot
  {
    std::string projection;
    explicit Snapshot(Fixture &f)
        : loka::testing::TextEditorContractSnapshot(f)
    {
      LOKA_VERIFY(f.node.document.project(this->projection) == EDITOR_OK);
    }
    void unchanged(Fixture &f) const
    {
      loka::testing::TextEditorContractSnapshot::unchanged(f);
      LOKA_VERIFY(f.native() == this->projection);
    }
  };
  struct Observer
  {
    Fixture &f;
    int count;
    bool nested;
    bool external;
    EditorResult result;
    Observer(Fixture &value)
        : f(value),
          count(0),
          nested(false),
          external(false),
          result(EDITOR_OK)
    {
      const_cast<State<ListRevision> &>(f.lines.revision()).bind(&changed, this, false);
    }
    ~Observer()
    {
      const_cast<State<ListRevision> &>(f.lines.revision()).unbind(&changed, this);
    }
    static void changed(void *p)
    {
      Observer &o = *static_cast<Observer *>(p);
      ++o.count;
      if (o.nested)
      {
        o.nested = false;
        const int sets = toolbox_host::sets;
        o.result = o.f.context->key('z');
        LOKA_VERIFY(toolbox_host::sets == sets);
      }
      if (o.external)
      {
        o.external = false;
        o.f.tracker.defer(&settled, &o);
      }
    }
    static void settled(void *p)
    {
      Observer &o = *static_cast<Observer *>(p);
      LOKA_VERIFY(o.f.lines.update(o.f.lines.at(1).id, String("owner")) == EDIT_OK);
      o.f.context->onPropsApplied();
    }
  };
  void pin(const char *s)
  {
    std::printf("[pin] %s\n", s);
    std::fflush(stdout);
  }
} // namespace
int main()
{
  {
    Fixture f;
    Point inside = {21, 11}, outside = {0, 0};
    LOKA_VERIFY(!(**f.te()).active);
    LOKA_VERIFY(f.controller.handleEditClick(inside));
    LOKA_VERIFY((**f.te()).active);
    f.controller.idleTextEdits();
    LOKA_VERIFY((**f.te()).idleCalls == 1);
    LOKA_VERIFY(!f.controller.handleEditClick(outside));
    LOKA_VERIFY(!f.controller.editControls_.focused() && !(**f.te()).active);
    f.controller.idleTextEdits();
    LOKA_VERIFY((**f.te()).idleCalls == 1);
    LOKA_VERIFY(f.controller.handleEditClick(inside));
    LOKA_VERIFY((**f.te()).active);
    f.controller.idleTextEdits();
    LOKA_VERIFY((**f.te()).idleCalls == 2);
    pin("focus activates; blur deactivates and receives no TEIdle; refocus activates and resumes TEIdle");
  }
  for (int stay = 0; stay < 2; ++stay)
  {
    const NativeLifetimeHint hint = stay ? NATIVE_HINT_DESIRE_STAY : NATIVE_HINT_DEFAULT;
    Fixture f;
    ToolboxEditTextContext ordinary;
    MutableState<String> text(String("ordinary"));
    const Rect rect = {20, 10, 40, 210};
    TEHandle editor = f.te();
    const int disposals = toolbox_host::disposals;
    f.controller.retireTextEditorControl(f.context, hint);
    LOKA_VERIFY(!f.te() && f.controller.editControls_.empty());
    LOKA_VERIFY(toolbox_host::disposals == disposals);
    f.controller.flushTE();
    LOKA_VERIFY(toolbox_host::disposals == disposals + 1);
    LOKA_VERIFY(f.controller.textEditBucket_.depth() == 0);
    GrafPtr previous;
    GetPort(&previous);
    SetPort(f.window.window());
    TEHandle edit = f.controller.ensureEditTextControl(&ordinary, rect, &text, hint);
    SetPort(previous);
    LOKA_VERIFY(edit && edit != editor);
    LOKA_VERIFY((**edit).txFont == 3 && (**edit).txSize == 12);
    LOKA_VERIFY((**edit).lineHeight == 17 && (**edit).fontAscent == 12);
    f.controller.retireEditTextControl(&ordinary, hint);
    f.controller.flushTE();
    LOKA_VERIFY(f.controller.textEditBucket_.depth() == 1);
    f.context->render(&f.controller);
    LOKA_VERIFY(f.te() && f.te() != edit && f.te() != editor);
    LOKA_VERIFY((**f.te()).txFont == 4 && (**f.te()).txSize == 9);
    LOKA_VERIFY((**f.te()).lineHeight == 14 && (**f.te()).fontAscent == 9);
    LOKA_VERIFY(f.controller.textEditBucket_.depth() == 1);
    // The ordinary record remains available only to another ordinary edit.
    SetPort(f.window.window());
    LOKA_VERIFY(f.controller.ensureEditTextControl(&ordinary, rect, &text, hint) == edit);
    SetPort(previous);
    f.controller.retireEditTextControl(&ordinary, hint);
    LOKA_VERIFY(f.controller.poolIntakeAuditFailCount_ == 0);
  }
  pin("default/desire-stay: editor disposes only at clock drain; cross-kind handles and font metrics stay isolated");
  {
    GrafPtr previous;
    GetPort(&previous);
    const short font = previous->txFont, size = previous->txSize;
    const Style face = previous->txFace;
    Fixture f;
    LOKA_VERIFY((**f.te()).txFont == 4 && (**f.te()).txSize == 9);
    LOKA_VERIFY((**f.te()).lineHeight == 14 && (**f.te()).fontAscent == 9);
    LOKA_VERIFY(f.window.port.txFont == 3 && f.window.port.txSize == 12
                && f.window.port.txFace == 0);
    GrafPtr restored;
    GetPort(&restored);
    LOKA_VERIFY(restored == previous && previous->txFont == font
                && previous->txSize == size && previous->txFace == face);
    pin("Monaco 9: TE captures font 4/size 9 and matching metrics; caller port restored");
  }
  {
    Fixture f;
    Observer o(f);
    const ListRevision before = f.lines.revision().get();
    LOKA_VERIFY(f.context->key('x') == EDITOR_OK && f.native() == "abxcd\rabcd\rabcd");
    LOKA_VERIFY(o.count == 1 && f.lines.revision().get().content == before.content + 1);
    LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_UPDATE);
    LOKA_VERIFY(f.context->key('\r') == EDITOR_OK && f.lines.size() == 4 && o.count == 2);
    LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
    LOKA_VERIFY(f.context->key('\b') == EDITOR_OK && f.lines.size() == 3 && o.count == 3);
    LOKA_VERIFY(f.lines.revision().get().change.kind == LIST_BATCH);
    Point p = {37, 16};
    LOKA_VERIFY(f.context->click(p) == EDITOR_OK && f.cursor.get() == LineCursor(f.lines.at(1).id, 1));
    LOKA_VERIFY(f.context->key(29) == EDITOR_OK && f.cursor.get().column == 2 && o.count == 3);
    pin("update once; split/join one batch; click and arrows publish caret only");
  }
  {
    Fixture f(200, std::string(39, 'a'));
    toolbox_host::copied = 0;
    const int sets = toolbox_host::sets, updates = toolbox_host::updates;
    LOKA_VERIFY(f.context->key('x') == EDITOR_OK);
    LOKA_VERIFY(toolbox_host::copied == 40);
    LOKA_VERIFY(toolbox_host::sets == sets && toolbox_host::updates == updates);
    pin("line slice: 40 copied bytes for 7999-byte document; no TESetText/TEUpdate on accepted key");
  }
  for (int kind = 0; kind < 3; ++kind)
  {
    Fixture f(kind == 0 ? 256 : 3);
    if (kind == 1)
    {
      LOKA_VERIFY(f.lines.detach() == EDIT_OK);
      LOKA_VERIFY(f.lines.attach(&f.tracker, 256) == ATTACH_OK);
      LOKA_VERIFY(f.lines.insert(0, String("fresh")) == EDIT_OK);
    }
    Observer o(f);
    Snapshot before(f);
    if (kind == 2)
      loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 1);
    LOKA_VERIFY(f.context->key(kind == 1 ? 'x' : '\r')
                == (kind == 0   ? EDITOR_CAPACITY
                    : kind == 1 ? EDITOR_STALE_ID
                                : EDITOR_ALLOCATION));
    before.unchanged(f);
    LOKA_VERIFY(f.restores() == 1 && o.count == 0);
    loka::core::testing::allowLokaAllocRaw();
    LOKA_VERIFY(f.context->key('y') == EDITOR_OK);
  }
  pin("capacity/stale ID/scratch refusal: unchanged facts, native restored once, next key succeeds");
  {
    Fixture f;
    Snapshot before(f);
    (**f.te()).destRect.top -= 32;
    (**f.te()).destRect.bottom -= 32;
    toolbox_host::failSets = 1;
    LOKA_VERIFY(f.context->paste(std::string(8193, 'x').data(), 8193) == EDITOR_CAPACITY);
    LOKA_VERIFY(ToolboxTextEditorAccess::status(*f.context) == EDITOR_UNAVAILABLE);
    LOKA_VERIFY(f.context->key('y') == EDITOR_UNAVAILABLE && f.restores() == 1);
    LOKA_VERIFY((**f.te()).destRect.top == -12);
    f.context->retryProjection();
    before.unchanged(f);
    LOKA_VERIFY(f.context->key('y') == EDITOR_OK);
    LayoutState state;
    state.x = 10;
    state.y = 20;
    state.width = 200;
    state.height = 80;
    f.context->layout(&f.controller, state);
    LOKA_VERIFY((**f.te()).destRect.top == -12);
    state.y = 40;
    state.width = 240;
    f.context->layout(&f.controller, state);
    LOKA_VERIFY((**f.te()).destRect.top == 8 && (**f.te()).viewRect.top == 40);
    pin("checked replacement blocks keys until idle retry; scroll survives restore and relayout");
  }
  {
    Fixture f;
    Observer o(f);
    o.nested = true;
    LOKA_VERIFY(f.context->key('x') == EDITOR_OK);
    LOKA_VERIFY(o.result == EDITOR_REENTRANT && o.count == 1 && f.restores() == 1);
    LOKA_VERIFY(f.native() == "abxcd\rabcd\rabcd");
    LOKA_VERIFY(f.context->key('y') == EDITOR_OK);
    const int disposals = toolbox_host::disposals;
    f.controller.editControls_.focus(0);
    f.context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
    LOKA_VERIFY(f.controller.editControls_.empty() && !f.controller.editControls_.focused());
    LOKA_VERIFY(!f.te() && f.controller.retiredTextEdits_.size() == 1 && toolbox_host::disposals == disposals);
    LOKA_VERIFY(f.context->key('x') == EDITOR_UNAVAILABLE);
    f.controller.flushTE();
    LOKA_VERIFY(toolbox_host::disposals == disposals + 1);
    pin("nested input rejected/reconciled; detach drops focus/row and defers disposal");
  }

  {
    Fixture f;
    Snapshot before(f);
    TESetSelect(2, 7, f.te());
    LOKA_VERIFY(f.context->key('x') == EDITOR_INVALID_CURSOR);
    before.unchanged(f);
    TESetSelect(1, 3, f.te());
    LOKA_VERIFY(f.context->key('\r') == EDITOR_INVALID_CURSOR);
    before.unchanged(f);
    LOKA_VERIFY(f.restores() == 2);
    pin("cross-line selection and selected Enter refuse without corrupting the model");
  }
  {
    Fixture f;
    ObservableList<String> other;
    LOKA_VERIFY(other.attach(&f.tracker, 256) == ATTACH_OK);
    for (unsigned short i = 0; i < 3; ++i)
      LOKA_VERIFY(other.insert(i, String("WXYZ")) == EDIT_OK);
    LOKA_VERIFY(!(other.revision().get() != f.lines.revision().get()));
    f.node.props.lines_ = &other;
    f.context->onPropsApplied();
    LOKA_VERIFY(f.native() == "WXYZ\rWXYZ\rWXYZ");
    f.node.props.lines_ = &f.lines;
    f.context->onPropsApplied();
    Observer observer(f);
    observer.external = true;
    LOKA_VERIFY(f.context->key('x') == EDITOR_OK);
    LOKA_VERIFY(f.native() == "abxcd\rowner\rabcd");
    LOKA_VERIFY(f.context->key('y') == EDITOR_OK);
    LOKA_VERIFY(f.native() == "abxycd\rowner\rabcd");
    pin("equal-revision props rebinding and owner settlement writes reconcile");
  }
  {
    Fixture f;
    const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
    LOKA_VERIFY(f.context->queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    SetRect(&f.controller.projectionClip, 30, 30, 80, 60);
    LayoutState state;
    state.x = 10;
    state.y = 20;
    state.width = 200;
    state.height = 80;
    f.context->layout(&f.controller, state);
    f.context->render(&f.controller);
    LOKA_VERIFY((**f.te()).viewRect.left == 30 && (**f.te()).viewRect.right == 80);
    LOKA_VERIFY((**f.te()).destRect.left == 10 && (**f.te()).destRect.right == 210);
    LOKA_VERIFY(f.controller.editControls_[0].rect.left == 30);
    f.controller.editControls_[0].usedThisFrame = false;
    SetRect(&f.controller.projectionClip, 300, 300, 310, 310);
    state.y = 20;
    f.context->layout(&f.controller, state);
    f.context->render(&f.controller);
    LOKA_VERIFY(!f.controller.editControls_[0].usedThisFrame);
    pin("native scheduled paint and real controller installation clip native view/hit bounds");
  }

  {
    Fixture f(1, std::string(8192, 'a'));
    Snapshot before(f);
    LOKA_VERIFY(f.context->key('x') == EDITOR_CAPACITY);
    before.unchanged(f);
    LOKA_VERIFY(f.restores() == 1);
    LOKA_VERIFY(f.context->key('\b') == EDITOR_OK);
    LOKA_VERIFY(f.context->key('x') == EDITOR_OK);
    pin("8192-byte cap refuses native insertion, restores, and permits recovery");
  }
  {
    Fixture f;
    Snapshot before(f);
    LOKA_VERIFY(f.context->key(static_cast<char>(0x80)) == EDITOR_NON_ASCII);
    before.unchanged(f);
    LOKA_VERIFY(f.context->paste("Q\r\nR\nS", 6) == EDITOR_OK);
    std::string projection;
    LOKA_VERIFY(f.node.document.project(projection) == EDITOR_OK && f.native() == projection);
    pin("non-ASCII refusal and CR/LF paste normalization");
  }
  {
    toolbox_host::failNew = 1;
    Fixture f;
    LOKA_VERIFY(!f.te() && ToolboxTextEditorAccess::status(*f.context) == EDITOR_UNAVAILABLE);
    LOKA_VERIFY(f.context->key('x') == EDITOR_UNAVAILABLE);
    f.context->render(&f.controller);
    LOKA_VERIFY(f.te() && f.context->key('x') == EDITOR_OK);
    toolbox_host::failSets = 2;
    LOKA_VERIFY(f.context->paste("", 8193) == EDITOR_CAPACITY);
    const int sets = toolbox_host::sets;
    f.context->retryProjection();
    LOKA_VERIFY(toolbox_host::sets == sets + 1 && ToolboxTextEditorAccess::status(*f.context) == EDITOR_UNAVAILABLE);
    f.context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
    f.context->retryProjection();
    LOKA_VERIFY(toolbox_host::sets == sets + 1);
    pin("TENew refusal is unavailable; persistent replacement costs one attempt per idle and detach cancels retry");
  }
  std::puts("Toolbox TextEditor host pins passed (fake TextEdit; native pixels pending)");
}
