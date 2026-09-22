#include "support/TextEditorStateOwner.hpp"
#include "support/TextEditorAccess.hpp"
#include "context/ToolboxTextEditorContext.hpp"
#include "ToolboxBuiltInSupport.hpp"
#include "context/ToolboxPaintSupport.hpp"
#include "support/TestVerify.hpp"
#include "support/TextEditorContractSnapshot.hpp"
#include "support/LokaAllocFailure.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "toolbox/ToolboxTextEditorAccess.hpp"
using namespace loka::app;
using namespace loka::app::scene;
using namespace loka::core;
using loka::testing::ToolboxTextEditorAccess;
namespace
{
  struct Fixture : loka::app::testing::TextEditorStateOwner
  {
    ObservableList<String> lines;
    ToolboxWindow window;
    ToolboxScenePlatformController controller;
    TextEditorNode node;
    ToolboxTextEditorContext *context;
    Fixture(unsigned short count = 3, const std::string &text = "abcd", unsigned short capacity = 256)
        : controller(&window),
          node(TextEditorProps(lines, cursor).moveCaretTo(request)),
          context(0)
    {
      LOKA_VERIFY(lines.attach(&tracker, capacity) == ATTACH_OK);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(lines.insert(i, String(text)) == EDIT_OK);
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
      // Seed native editing through its input seam; request tests post explicitly.
      if (count && ToolboxTextEditorAccess::status(*context) == EDITOR_OK)
      {
        Point initial = {
            static_cast<short>((**this->te()).viewRect.top),
            static_cast<short>((**this->te()).viewRect.left + 6 * std::min(2, static_cast<int>(text.size())))};
        LOKA_VERIFY(context->click(initial) == EDITOR_OK);
      }
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
  // Run the same key through an isolated fake TE before the rail sees it.
  // Expectations come from native text/selection, never offsetOf/cursorAt.
  void nativeKey(Fixture &f, char key)
  {
    TERec expected = **f.te();
    TERec *record = &expected;
    TEKey(key, &record);
    unsigned short row = 0;
    short start = 0;
    for (short i = 0; i < expected.selStart; ++i)
      if (expected.text[i] == '\r')
      {
        ++row;
        start = i + 1;
      }
    const int selections = toolbox_host::selections;
    LOKA_VERIFY(f.context->key(key) == EDITOR_OK);
    f.context->onPropsApplied();
    LOKA_VERIFY(toolbox_host::selections == selections);
    LOKA_VERIFY(f.native() == expected.text);
    LOKA_VERIFY((**f.te()).selStart == expected.selStart);
    LOKA_VERIFY((**f.te()).selEnd == expected.selEnd);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(row).id, expected.selStart - start));
    std::string committed;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).project(committed) == EDITOR_OK);
    LOKA_VERIFY(committed == expected.text);
  }
  struct Snapshot : loka::testing::TextEditorContractSnapshot
  {
    std::string projection;
    explicit Snapshot(Fixture &f)
        : loka::testing::TextEditorContractSnapshot(f)
    {
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).project(this->projection) == EDITOR_OK);
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
  struct RequestObserver
  {
    Fixture &f;
    LineCursor repost;
    bool onClear, onReport;
    unsigned requests, reports;
    RequestObserver(Fixture &fixture)
        : f(fixture),
          repost(),
          onClear(false),
          onReport(false),
          requests(0),
          reports(0)
    {
      f.request.state()->bind(&requestChanged, this, false);
      f.cursor.state()->bind(&factChanged, this, false);
    }
    ~RequestObserver()
    {
      f.request.state()->unbind(&requestChanged, this);
      f.cursor.state()->unbind(&factChanged, this);
    }
    static void requestChanged(void *value)
    {
      RequestObserver &o = *static_cast<RequestObserver *>(value);
      ++o.requests;
      LOKA_VERIFY(o.requests < 20);
      if (o.onClear && o.f.request.get().isNone())
      {
        o.onClear = false;
        o.f.request.set(o.repost);
        const int selections = toolbox_host::selections;
        o.f.context->onPropsApplied();
        LOKA_VERIFY(toolbox_host::selections == selections);
      }
    }
    static void factChanged(void *value)
    {
      RequestObserver &o = *static_cast<RequestObserver *>(value);
      ++o.reports;
      if (o.onReport)
      {
        LOKA_VERIFY(o.f.request.get().isNone());
        o.onReport = false;
        o.f.request.set(o.repost);
        const int selections = toolbox_host::selections;
        o.f.context->onPropsApplied();
        LOKA_VERIFY(toolbox_host::selections == selections);
        LOKA_VERIFY(o.f.request.get() == o.repost);
      }
    }
  };
  /** Alternate cursor values so every take produces a report. The bound
      assertion stops an unbounded consumer before it can exhaust memory. */
  struct RepostingObserver
  {
    Fixture &f;
    const int repostLimit;
    unsigned delivered;
    unsigned reports;
    RepostingObserver(Fixture &fixture, int limit)
        : f(fixture), repostLimit(limit), delivered(0), reports(0)
    {
      f.cursor.state()->bind(&changed, this, false);
    }
    ~RepostingObserver()
    {
      f.cursor.state()->unbind(&changed, this);
    }
    static void changed(void *value)
    {
      RepostingObserver &o = *static_cast<RepostingObserver *>(value);
      ++o.delivered;
      LOKA_VERIFY(o.delivered <= 2);
      ++o.reports;
      LOKA_VERIFY(o.f.request.get().isNone());
      if (o.repostLimit < 0 || o.reports <= static_cast<unsigned>(o.repostLimit))
      {
        o.f.request.set(LineCursor(o.f.lines.at(1).id, o.f.cursor.state()->get().column == 1 ? 2 : 1));
        o.f.context->onPropsApplied();
        LOKA_VERIFY(!o.f.request.get().isNone());
      }
    }
  };
  /** Synchronous app work invalidating a borrowed binding or native offsets. */
  struct RequestAction
  {
    enum Action
    {
      EDIT_ON_TAKE,
      EDIT_ON_REPORT,
      DETACH_ON_TAKE,
      REBIND_ON_TAKE,
      NESTED_ON_TAKE
    };
    Fixture &f;
    const Action action;
    bool armed;
    RequestAction(Fixture &fixture, Action value)
        : f(fixture),
          action(value),
          armed(true)
    {
      f.request.state()->bind(&taken, this, false);
      f.cursor.state()->bind(&reported, this, false);
    }
    ~RequestAction()
    {
      f.request.state()->unbind(&taken, this);
      f.cursor.state()->unbind(&reported, this);
    }
    static void taken(void *value)
    {
      RequestAction &o = *static_cast<RequestAction *>(value);
      if (!o.armed || !o.f.request.get().isNone() || o.action == EDIT_ON_REPORT)
        return;
      o.armed = false;
      switch (o.action)
      {
      case EDIT_ON_TAKE:
        LOKA_VERIFY(o.f.lines.update(o.f.lines.at(0).id, String("longer")) == EDIT_OK);
        break;
      case EDIT_ON_REPORT:
        break;
      case DETACH_ON_TAKE:
        NotifySubtreeNodeDetached(&o.f.node);
        break;
      case REBIND_ON_TAKE:
      {
        const bool applied =
            NodePropsApplier<TextEditorNode, TextEditorProps>::apply(&o.f.node, TextEditorProps(o.f.lines, o.f.cursor));
        LOKA_VERIFY(applied);
        break;
      }
      case NESTED_ON_TAKE:
        LOKA_VERIFY(o.f.context->key('x') == EDITOR_REENTRANT);
        break;
      }
      o.f.context->onPropsApplied();
    }
    static void reported(void *value)
    {
      RequestAction &o = *static_cast<RequestAction *>(value);
      if (o.action == EDIT_ON_TAKE)
      {
        LOKA_VERIFY(o.f.native() == "longer\rabcd\rabcd");
        LOKA_VERIFY((**o.f.te()).selStart == 10);
      }
      if (!o.armed || o.action != EDIT_ON_REPORT)
        return;
      o.armed = false;
      LOKA_VERIFY(o.f.lines.update(o.f.lines.at(0).id, String("longer")) == EDIT_OK);
      o.f.context->onPropsApplied();
    }
  };
  void invalidated(void *value)
  {
    ++*static_cast<unsigned *>(value);
  }
  void post(Fixture &f, LineCursor cursor)
  {
    StateTrackerGuard guard(&f.tracker);
    f.request.set(cursor);
  }
  void backspaceAtOrigin(void *value)
  {
    Fixture &f = *static_cast<Fixture *>(value);
    // An origin no-op must not enter the seam during an owner publication.
    LOKA_VERIFY(f.context->key('\b') == EDITOR_OK);
  }
  void pin(const char *s)
  {
    std::printf("[pin] %s\n", s);
    std::fflush(stdout);
  }
} // namespace
int main(int argc, char **argv)
{
  if (argc == 2 && std::strcmp(argv[1], "request") == 0)
  {
    {
      Fixture f;
      pin("PR2 acceptance: app request is consumed and moves native caret after a commit");
      nativeKey(f, '\b');
      const LineCursor desired(f.lines.at(1).id, 2);
      {
        StateTrackerGuard guard(&f.tracker);
        f.request.set(desired);
      }
      f.context->onPropsApplied();
      LOKA_VERIFY(f.request.get().isNone());
      const short expected = static_cast<short>(f.native().find('\r') + 1 + desired.column);
      LOKA_VERIFY((**f.te()).selStart == expected && (**f.te()).selEnd == expected);
      LOKA_VERIFY(f.cursor.state()->get() == desired);
      nativeKey(f, 28);
    }
    return 0;
  }

  if (argc == 2)
  {
    Fixture f;
    const LineCursor desired(f.lines.at(1).id, 3);
    if (std::strcmp(argv[1], "report") == 0)
    {
      pin("fact publication never echoes to native; delete and arrows cross CR");
      Point point = {37, 22};
      LOKA_VERIFY(f.context->click(point) == EDITOR_OK);
      nativeKey(f, '\b');
      nativeKey(f, 28);
      nativeKey(f, 28);
      nativeKey(f, 29);
      const short native = (**f.te()).selStart;
      const int selections = toolbox_host::selections;
      LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).moveCaret(desired) == EDITOR_OK);
      f.context->onPropsApplied();
      LOKA_VERIFY(toolbox_host::selections == selections);
      LOKA_VERIFY((**f.te()).selStart == native);
      LOKA_VERIFY(f.cursor.state()->get() == desired);
    }
    else if (std::strcmp(argv[1], "project") == 0)
    {
      pin("native creation project tail consumes request");
      f.controller.retireTextEditorControl(f.context, NATIVE_HINT_DEFAULT);
      post(f, desired);
      f.context->render(&f.controller);
      LOKA_VERIFY(f.request.get().isNone());
      LOKA_VERIFY((**f.te()).selStart == 8);
      LOKA_VERIFY(f.cursor.state()->get() == desired);
    }
    else if (std::strcmp(argv[1], "input") == 0)
    {
      pin("input completion delivers report-time request after cache completion");
      RequestObserver observer(f);
      observer.repost = desired;
      observer.onReport = true;
      LOKA_VERIFY(f.context->key(29) == EDITOR_OK);
      LOKA_VERIFY(f.request.get().isNone());
      LOKA_VERIFY((**f.te()).selStart == 8);
      LOKA_VERIFY(f.cursor.state()->get() == desired);
    }
    else if (std::strcmp(argv[1], "clear") == 0 || std::strcmp(argv[1], "epilogue") == 0)
    {
      pin("take precedes apply and finite synchronous repost survives to outer completion");
      RequestObserver observer(f);
      observer.repost = LineCursor(f.lines.at(2).id, 1);
      observer.onClear = std::strcmp(argv[1], "clear") == 0;
      observer.onReport = !observer.onClear;
      post(f, desired);
      f.context->onPropsApplied();
      LOKA_VERIFY(f.request.get().isNone());
      LOKA_VERIFY(f.cursor.state()->get() == observer.repost);
      LOKA_VERIFY((**f.te()).selStart == 11);
      LOKA_VERIFY(observer.requests == 4 && observer.reports == 2);
    }
    else if (std::strcmp(argv[1], "endless") == 0 || std::strcmp(argv[1], "five-reposts") == 0)
    {
      const bool endless = std::strcmp(argv[1], "endless") == 0;
      pin(endless ? "endless repost yields after two reports per delivery"
                  : "five reposts drain over three explicit deliveries");
      RepostingObserver observer(f, endless ? -1 : 5);
      post(f, LineCursor(f.lines.at(1).id, 1));
      for (unsigned delivery = 0; delivery < 3; ++delivery)
      {
        observer.delivered = 0;
        f.context->onPropsApplied();
        LOKA_VERIFY(observer.delivered == 2);
        LOKA_VERIFY(observer.reports == 2 * (delivery + 1));
        LOKA_VERIFY(f.request.get().isNone() == (!endless && delivery == 2));
        LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 2));
        LOKA_VERIFY((**f.te()).selStart == 7);
      }
    }
    else if (std::strcmp(argv[1], "refuse") == 0)
    {
      pin("missing native and failed projection take without reporting");
      const LineCursor before = f.cursor.state()->get();
      f.controller.retireTextEditorControl(f.context, NATIVE_HINT_DEFAULT);
      post(f, desired);
      f.context->onPropsApplied();
      LOKA_VERIFY(f.request.get().isNone());
      LOKA_VERIFY(f.cursor.state()->get() == before);
      toolbox_host::failNew = 1;
      post(f, desired);
      f.context->render(&f.controller);
      LOKA_VERIFY(f.request.get().isNone() && !f.te());
      LOKA_VERIFY(f.cursor.state()->get() == before);
      toolbox_host::failSets = 1;
      post(f, desired);
      f.context->render(&f.controller);
      LOKA_VERIFY(f.request.get().isNone());
      LOKA_VERIFY(f.cursor.state()->get() == before);
      f.context->retryProjection();
      LOKA_VERIFY(f.cursor.state()->get() == before);
    }
    else if (std::strcmp(argv[1], "take-edit") == 0 || std::strcmp(argv[1], "report-edit") == 0
             || std::strcmp(argv[1], "detach") == 0 || std::strcmp(argv[1], "rebind") == 0
             || std::strcmp(argv[1], "nested") == 0)
    {
      pin("callback invalidation is reconciled under exclusion before next delivery");
      const RequestAction::Action action = std::strcmp(argv[1], "take-edit") == 0     ? RequestAction::EDIT_ON_TAKE
                                           : std::strcmp(argv[1], "report-edit") == 0 ? RequestAction::EDIT_ON_REPORT
                                           : std::strcmp(argv[1], "detach") == 0      ? RequestAction::DETACH_ON_TAKE
                                           : std::strcmp(argv[1], "rebind") == 0      ? RequestAction::REBIND_ON_TAKE
                                                                                      : RequestAction::NESTED_ON_TAKE;
      RequestAction observer(f, action);
      const LineCursor before = f.cursor.state()->get();
      post(f, desired);
      f.context->onPropsApplied();
      LOKA_VERIFY(f.request.get().isNone());
      if (action == RequestAction::EDIT_ON_TAKE)
      {
        LOKA_VERIFY(f.native() == "longer\rabcd\rabcd");
        LOKA_VERIFY((**f.te()).selStart == 10 && f.cursor.state()->get() == desired);
      }
      else if (action == RequestAction::EDIT_ON_REPORT)
      {
        LOKA_VERIFY(f.native() == "longer\rabcd\rabcd");
        LOKA_VERIFY((**f.te()).selStart == 10);
        LOKA_VERIFY(f.cursor.state()->get() == desired);
      }
      else
        LOKA_VERIFY(f.cursor.state()->get() == before);
    }
    else if (std::strcmp(argv[1], "retry") == 0)
    {
      pin("retry project completion delivers; unavailable input refuses without report");
      const LineCursor before = f.cursor.state()->get();
      toolbox_host::failSets = 1;
      LOKA_VERIFY(f.context->paste(std::string(8193, 'x').data(), 8193) == EDITOR_CAPACITY);
      post(f, desired);
      LOKA_VERIFY(f.context->key('x') == EDITOR_UNAVAILABLE);
      LOKA_VERIFY(f.request.get().isNone() && f.cursor.state()->get() == before);
      post(f, desired);
      f.context->retryProjection();
      LOKA_VERIFY(f.request.get().isNone());
      LOKA_VERIFY((**f.te()).selStart == 8 && f.cursor.state()->get() == desired);
    }
    else if (std::strcmp(argv[1], "clamp") == 0)
    {
      pin("request clamps columns, refuses stale identities, and leaves None silent");
      RequestObserver observer(f);
      post(f, LineCursor(desired.line, 99));
      f.context->onPropsApplied();
      LOKA_VERIFY(f.request.get().isNone());
      LOKA_VERIFY(f.cursor.state()->get() == LineCursor(desired.line, 4));
      LOKA_VERIFY((**f.te()).selStart == 9);
      const unsigned requests = observer.requests, reports = observer.reports;
      const int selections = toolbox_host::selections;
      unsigned invalidations = 0;
      f.tracker.setInvalidateCallback(&invalidated, &invalidations);
      post(f, LineCursor::None());
      LOKA_VERIFY(invalidations == 1);
      invalidations = 0;
      f.context->onPropsApplied();
      LOKA_VERIFY(invalidations == 0);
      f.tracker.setInvalidateCallback(0, 0);
      LOKA_VERIFY(observer.requests == requests && observer.reports == reports);
      LOKA_VERIFY(toolbox_host::selections == selections);
      post(f, LineCursor(desired.line, 99));
      f.context->onPropsApplied();
      LOKA_VERIFY(f.request.get().isNone() && observer.requests == requests + 2);
      LOKA_VERIFY(observer.reports == reports && toolbox_host::selections == selections + 1);
      const int afterRepeat = toolbox_host::selections;
      post(f, LineCursor(ItemId(123, 456), 1));
      f.context->onPropsApplied();
      LOKA_VERIFY(f.request.get().isNone());
      LOKA_VERIFY(observer.reports == reports && toolbox_host::selections == afterRepeat);
    }
    else
      LOKA_VERIFY(false && "unknown request test");
    return 0;
  }

  for (int atEnd = 0; atEnd < 2; ++atEnd)
  {
    Fixture f;
    pin(atEnd ? "delete at line 2 end, then Left" : "delete inside line 2, Left across CR, Right back");
    const short column = atEnd ? 4 : 2;
    Point point = {37, static_cast<short>(10 + column * 6)};
    LOKA_VERIFY(f.context->click(point) == EDITOR_OK);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, column));
    const ListRevision before = f.lines.revision().get();
    nativeKey(f, '\b');
    LOKA_VERIFY(f.lines.revision().get().content == before.content + 1);
    if (atEnd)
      nativeKey(f, 28);
    else
    {
      for (short i = 0; i < column; ++i)
        nativeKey(f, 28);
      nativeKey(f, 29);
    }
  }

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
    LOKA_VERIFY(f.context->click(p) == EDITOR_OK && f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 1));
    LOKA_VERIFY(f.context->key(29) == EDITOR_OK && f.cursor.state()->get().column == 2 && o.count == 3);
    pin("update once; split/join one batch; click and arrows publish caret only");
  }
  {
    Fixture f(200, std::string(39, 'a'));
    toolbox_host::copied = 0;
    const int sets = toolbox_host::sets, updates = toolbox_host::updates;
    LOKA_VERIFY(f.context->key('x') == EDITOR_OK);
    LOKA_VERIFY(toolbox_host::copied == 0);
    LOKA_VERIFY(toolbox_host::sets == sets && toolbox_host::updates == updates);
    pin("range door: zero copied bytes for 7999-byte document; no TESetText/TEUpdate on accepted key");
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
    LOKA_VERIFY(f.lines.detach() == EDIT_OK);
    LOKA_VERIFY(f.lines.attach(&f.tracker, 256) == ATTACH_OK);
    LOKA_VERIFY(f.lines.insert(0, String("fresh")) == EDIT_OK);
    Snapshot before(f);
    LOKA_VERIFY(f.context->paste("x", 1) == EDITOR_STALE_ID);
    before.unchanged(f);
    LOKA_VERIFY(f.restores() == 1);
    LOKA_VERIFY(f.context->paste("y", 1) == EDITOR_OK && f.native() == "fryesh");
    pin("paste refuses stale projected identity, restores current rows, then recovers");
  }
  {
    // The owner inserts a row above the caret without touching the caret's
    // line: every cached identity survives, but TE's offsets now name the
    // wrong rows. Both doors must refuse on the revision, not on the identity.
    Fixture f;
    LOKA_VERIFY(f.lines.insert(0, String("above")) == EDIT_OK);
    Snapshot before(f);
    LOKA_VERIFY(f.context->key('x') == EDITOR_STALE_ID);
    before.unchanged(f);
    LOKA_VERIFY(f.restores() == 1);
    // The restore reprojects the current rows, so the next input is current again.
    LOKA_VERIFY(f.context->paste("y", 1) == EDITOR_OK && f.restores() == 1);
    pin("unprojected owner row insertion above the caret refuses the key on the revision, then recovers");
  }
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

  for (int action = 0; action < 4; ++action)
  {
    Fixture f(3, "abcd", 3);
    const ItemId first = f.lines.at(0).id;
    const int sets = toolbox_host::sets;
    Observer observer(f);
    TESetSelect(0, 32767, f.te());
    const EditorResult result = action == 3 ? f.context->paste("Q\r\nR", 4)
                                            : f.context->key(action == 0   ? 'b'
                                                             : action == 1 ? '\b'
                                                                           : '\r');
    LOKA_VERIFY(result == EDITOR_OK);
    const std::string expected = action == 0 ? "b" : action == 1 ? "" : action == 2 ? "\r" : "Q\rR";
    std::string projection;
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).project(projection) == EDITOR_OK
                && projection == expected);
    LOKA_VERIFY(f.native() == expected && f.lines.size() == (action < 2 ? 1 : 2));
    LOKA_VERIFY(f.lines.at(0).id == first && observer.count == 1 && f.restores() == 0);
    LOKA_VERIFY(f.cursor.state()->get()
                == LineCursor(f.lines.at(action < 2 ? 0 : 1).id, action == 0 || action == 3 ? 1 : 0));
    LOKA_VERIFY(action == 3 || toolbox_host::sets == sets);
  }
  pin("three-line selection: type, Backspace, Return and paste replace at full capacity; keys never rewrite TE");
  {
    Fixture f;
    TESetSelect(1, 12, f.te());
    const int sets = toolbox_host::sets;
    LOKA_VERIFY(f.context->key('b') == EDITOR_OK && f.native() == "abcd");
    LOKA_VERIFY(f.lines.size() == 1 && f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 2));
    LOKA_VERIFY(toolbox_host::sets == sets);
    TESetSelect(1, 3, f.te());
    LOKA_VERIFY(f.context->key('\r') == EDITOR_OK && f.native() == "a\rd");
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 0));
    LOKA_VERIFY(toolbox_host::sets == sets);
    pin("partial selection preserves prefix/suffix; selected Return splits at its start");
  }
  for (int paste = 0; paste < 2; ++paste)
  {
    Fixture f(3, "abcd", 3);
    Snapshot before(f);
    TESetSelect(1, 3, f.te());
    const unsigned restores = f.restores();
    LOKA_VERIFY((paste ? f.context->paste("Q\rR", 3) : f.context->key('\r')) == EDITOR_CAPACITY);
    before.unchanged(f);
    LOKA_VERIFY(f.restores() == restores + 1);
  }
  pin("capacity refusal with a selection restores native text once for Return and paste");
  {
    Fixture f;
    const int sets = toolbox_host::sets;
    TESetSelect(7, 7, f.te());
    LOKA_VERIFY(f.context->key('\b') == EDITOR_OK && f.native() == "abcd\racd\rabcd");
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(1).id, 1));
    TESetSelect(5, 5, f.te());
    LOKA_VERIFY(f.context->key('\b') == EDITOR_OK && f.native() == "abcdacd\rabcd");
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 4));
    TESetSelect(0, 0, f.te());
    LOKA_VERIFY(f.context->key(28) == EDITOR_OK);
    Snapshot before(f);
    Observer observer(f);
    LOKA_VERIFY(f.context->key('\b') == EDITOR_OK);
    before.unchanged(f);
    LOKA_VERIFY(observer.count == 0 && toolbox_host::sets == sets);
    State<ListRevision> &revision = const_cast<State<ListRevision> &>(f.lines.revision());
    revision.bind(&backspaceAtOrigin, &f, false);
    LOKA_VERIFY(f.lines.update(f.lines.at(0).id, String("abcdacd")) == EDIT_OK);
    revision.unbind(&backspaceAtOrigin, &f);
    LOKA_VERIFY(f.cursor.state()->get() == LineCursor(f.lines.at(0).id, 0));
    pin("empty-selection Backspace uses native endpoint, joins predecessor, and origin publishes nothing");
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
    LOKA_VERIFY(loka::app::testing::TextEditorAccess::document(f.node).project(projection) == EDITOR_OK
                && f.native() == projection);
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
