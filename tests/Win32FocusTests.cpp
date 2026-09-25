#include "Win32FocusTests.hpp"
#include "Win32App.hpp"
#include "Win32Window.hpp"
#include "Win32ScenePlatformController.hpp"
#include "context/Win32ButtonContext.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32TextEditorContext.hpp"
#include "context/Win32FocusParticipant.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "support/Headless.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/TestVerify.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  typedef Focused<unsigned short> Fact;
  struct Facts : HeadlessStateOwner
  {
    Reported<Fact> focus;
    Reported<LineCursor> cursor;
    ObservableList<String> lines;
    Facts()
    {
      StateBatchBase::CreateImmediateState(this, this->focus, Fact::none());
      StateBatchBase::CreateImmediateState(this, this->cursor, LineCursor::None());
      // TextEditor borrows a non-null app-owned list (TextEditor.hpp).
      LOKA_VERIFY(this->lines.attach(this->tracker()->asPushTracker(), 4) == ATTACH_OK);
      LOKA_VERIFY(this->lines.insert(0, String("focus")) == EDIT_OK);
    }
  };
  Facts *composingFacts = 0;
  class FocusRoot : public BoundaryNodeFor<FocusRoot>
  {
  public:
    explicit FocusRoot(const BoundaryPropsFor<FocusRoot> &p) : BoundaryNodeFor<FocusRoot>(p) {}
    virtual void composeNode(NodeComposition &c)
    {
      // A top-level declare() sets the composition root, so the four controls
      // are one Column; its children are the tab order field(0..3) walks.
      c.declare(Column()
                << Button()
                << EditText(EditTextProps().focusedAs(composingFacts->focus, static_cast<unsigned short>(1)))
                << EditText(EditTextProps().focusedAs(composingFacts->focus, static_cast<unsigned short>(2)))
                << TextEditor(TextEditorProps(composingFacts->lines, composingFacts->cursor)
                                  .focusedAs(composingFacts->focus, static_cast<unsigned short>(3))));
    }
  };

  /** LOKA_WIN32_REFUSE_ACTIVATION=1 stands in for a desktop that never makes
      the test window active: a thread CBT hook refuses every HCBT_ACTIVATE
      for the fixture's lifetime, so the pins take their [skip] branch on any
      rig. Unset, it installs nothing. */
  class ActivationRefusal
  {
  public:
    ActivationRefusal() : hook_(0)
    {
      const char *requested = std::getenv("LOKA_WIN32_REFUSE_ACTIVATION");
      if (requested && std::strcmp(requested, "1") == 0)
      {
        this->hook_ = SetWindowsHookExW(WH_CBT, &ActivationRefusal::refuse, 0, GetCurrentThreadId());
        LOKA_VERIFY(this->hook_);
      }
    }
    ~ActivationRefusal()
    {
      if (this->hook_)
        UnhookWindowsHookEx(this->hook_);
    }

  private:
    static LRESULT CALLBACK refuse(int code, WPARAM wParam, LPARAM lParam)
    {
      return code == HCBT_ACTIVATE ? 1 : CallNextHookEx(0, code, wParam, lParam);
    }
    HHOOK hook_;
    ActivationRefusal(const ActivationRefusal &);
    ActivationRefusal &operator=(const ActivationRefusal &);
  };

  struct Fixture
  {
    ActivationRefusal refusal; // Precedes window: its first show is refused too.
    Facts facts;
    NullPlatformContext context;
    Win32Window window;
    WindowAdmissionTestApp app;
    static WindowProps props(Facts &facts, bool idle)
    {
      composingFacts = &facts;
      WindowProps p;
      p.frame(60, 60, 400, 300).visible(true).scene(new Scene(Boundary<FocusRoot>()));
      if (idle)
        p.idlePolicy(IdlePolicy::everyTick());
      return p;
    }
    explicit Fixture(bool idle = false)
        : window(&this->context, props(this->facts, idle)), app(this->window)
    {
      this->app.flush();
      LOKA_VERIFY(this->window.hwnd());
    }
    /** Activation is a desktop grant, not Loka behavior. False sends a pin to
        its [skip] branch, where only checks that need no active window run. */
    bool activate()
    {
      SetActiveWindow(this->window.hwnd());
      SetFocus(this->window.hwnd());
      return GetActiveWindow() == this->window.hwnd();
    }
    Node *field(unsigned index)
    {
      BoundaryNode *root = loka::dsl::testing::SceneTestAccess::rootBoundary(*this->window.scene());
      LOKA_VERIFY(root && root->childrenHead() && root->childrenHead()->asNestable());
      Node *node = root->childrenHead()->asNestable()->childrenHead();
      while (index-- && node)
        node = node->nextInComposition;
      LOKA_VERIFY(node);
      return node;
    }
    HWND edit(unsigned index)
    {
      NodeContext *context = this->field(index)->getContext();
      LOKA_VERIFY(context);
      return index == 3 ? static_cast<Win32TextEditorContext *>(context)->hwnd()
                        : static_cast<Win32EditTextContext *>(context)->hwnd();
    }
    Win32ScenePlatformController &rail()
    {
      return *static_cast<Win32ScenePlatformController *>(
          loka::dsl::testing::SceneTestAccess::platformController(*this->window.scene()));
    }
    /** Active: the rail answers and names no participant. Inactive: it
        declines. The preset answer is non-null so a stale out-value shows. */
    void readsNoParticipant(bool active)
    {
      NodeContext *answer = this->field(1)->getContext();
      if (active)
        LOKA_VERIFY(this->rail().readNativeFocus(answer) && !answer);
      else
        LOKA_VERIFY(!this->rail().readNativeFocus(answer));
    }
    void none() { LOKA_VERIFY(!(this->facts.focus.state()->get() != Fact::none())); }
    void expect(unsigned short key) { LOKA_VERIFY(this->facts.focus.state()->get().is(key)); }
  };

  void skipWithoutActivation(const char *pin, const char *skipped, const char *kept)
  {
    std::printf("[skip] %s: SetActiveWindow did not make the test window this thread's active window, "
                "so %s did not run; %s\n",
                pin, skipped, kept);
    std::fflush(stdout);
  }

  void tab(HWND root)
  {
    MSG msg = {};
    msg.hwnd = GetFocus();
    msg.message = WM_KEYDOWN;
    msg.wParam = VK_TAB;
    LOKA_VERIFY(IsDialogMessageW(root, &msg));
  }

  /** Needs no activation: each participant HWND bears its own context's mark
      from attach, and the Button, which has GWLP_USERDATA, bears none. */
  void verifyMarks(Fixture &f)
  {
    NodeContext *button = f.field(0)->getContext();
    LOKA_VERIFY(button);
    LOKA_VERIFY(!Win32FocusParticipant::read(static_cast<Win32ButtonContext *>(button)->hwnd()));
    for (unsigned key = 1; key <= 3; ++key)
      LOKA_VERIFY(Win32FocusParticipant::read(f.edit(key)) == f.field(key)->getContext());
  }

  /** Detach removes the mark and hides, reattach restores the mark, and
      retirement removes it before the deferred native destruction; none of
      that needs activation. An active window also reads and publishes at
      each step; an inactive one declines every read. */
  void verifyLifecycle(Fixture &f, bool active)
  {
    for (unsigned short key = 1; key <= 3; ++key)
    {
      Node *node = f.field(key);
      const HWND retired = f.edit(key);
      if (active)
      {
        SetFocus(retired);
        f.app.reconcileFocus();
        f.expect(key);
      }
      NotifySubtreeNodeDetached(node);
      LifecycleFactTestAccess::DeliverFacts(node);
      LOKA_VERIFY(!Win32FocusParticipant::read(retired));
      LOKA_VERIFY(!IsWindowVisible(retired));
      if (active)
        SetFocus(retired); // A hidden HWND must not provide a read source either.
      f.readsNoParticipant(active);
      f.app.reconcileFocus();
      f.none();
      NotifySubtreeNodeAttached(node);
      LifecycleFactTestAccess::DeliverFacts(node);
      LOKA_VERIFY(Win32FocusParticipant::read(retired) == node->getContext());
      if (active)
      {
        SetFocus(retired);
        f.app.reconcileFocus();
        f.expect(key);
      }
      LifecycleFactTestAccess::MarkSubtreeRetired(node);
      LifecycleFactTestAccess::DeliverFacts(node);
      LOKA_VERIFY(IsWindow(retired)); // Native destruction is deferred.
      LOKA_VERIFY(!Win32FocusParticipant::read(retired));
      f.readsNoParticipant(active);
      f.none();
    }
  }
}

void testWin32FocusReadAndRestore()
{
  Fixture f;
  verifyMarks(f);
  if (!f.activate())
  {
    skipWithoutActivation("testWin32FocusReadAndRestore",
                          "the Tab, publication, HELD and reactivation checks",
                          "the participant marks, the inactive read and the lifecycle marks still run.");
    f.readsNoParticipant(false);
    f.app.reconcileFocus();
    f.none(); // A declined read publishes nothing.
    verifyLifecycle(f, false);
    return;
  }
  LOKA_VERIFY(GetFocus() == f.window.hwnd());
  f.readsNoParticipant(true);
  f.app.reconcileFocus();
  f.none();
  // Negative control: even a marked root is not a descendant participant.
  LOKA_VERIFY(Win32FocusParticipant::attach(f.window.hwnd(), f.field(1)->getContext()));
  f.readsNoParticipant(true);
  Win32FocusParticipant::detach(f.window.hwnd());
  tab(f.window.hwnd());
  LOKA_VERIFY(GetFocus() != f.window.hwnd());
  LOKA_VERIFY(!Win32FocusParticipant::read(GetFocus()));
  f.app.reconcileFocus();
  f.none(); // Button has userdata, but no participant atom.
  NodeContext *answer = 0;
  for (unsigned short key = 1; key <= 3; ++key)
  {
    tab(f.window.hwnd());
    LOKA_VERIFY(GetFocus() == f.edit(key));
    LOKA_VERIFY(f.rail().readNativeFocus(answer) && answer == f.field(key)->getContext());
    f.app.reconcileFocus();
    f.expect(key);
  }

  // A second real top-level window exercises HELD and native activation restore.
  Win32Window other(&f.context, WindowProps().frame(500, 60, 160, 120).visible(true));
  WindowAdmissionTestApp otherApp(other);
  otherApp.flush();
  for (unsigned short key = 1; key <= 3; ++key)
  {
    SetActiveWindow(f.window.hwnd());
    SetFocus(f.edit(key));
    f.app.reconcileFocus();
    f.expect(key);
    SetActiveWindow(other.hwnd());
    SetFocus(other.hwnd());
    LOKA_VERIFY(GetActiveWindow() == other.hwnd());
    f.readsNoParticipant(false);
    f.app.reconcileFocus();
    f.expect(key);
    SetActiveWindow(f.window.hwnd());
    LOKA_VERIFY(GetFocus() == f.edit(key));
    f.app.reconcileFocus();
    f.expect(key);
  }
  // Reactivation restores only an HWND that still bears the published
  // context's mark; the fact itself stays HELD until the next read.
  const HWND editor = f.edit(3);
  Win32FocusParticipant::detach(editor);
  SetActiveWindow(other.hwnd());
  SetFocus(other.hwnd());
  SetActiveWindow(f.window.hwnd());
  LOKA_VERIFY(GetActiveWindow() == f.window.hwnd());
  LOKA_VERIFY(GetFocus() != editor);
  f.expect(3);
  LOKA_VERIFY(Win32FocusParticipant::attach(editor, f.field(3)->getContext()));
  // The WM_ACTIVATE minimized bit must suppress restore even before IsIconic
  // catches up. A synthetic message isolates that guard from OS focus policy.
  SetFocus(f.window.hwnd());
  SendMessageW(f.window.hwnd(), WM_ACTIVATE, MAKEWPARAM(WA_ACTIVE, TRUE), 0);
  LOKA_VERIFY(GetFocus() != f.edit(3));
  // Deactivation never restores. A real WA_INACTIVE arrives while this window
  // is still the active one, so a restore there would take focus until the
  // new active window overrides it; the synthetic message keeps it visible.
  SendMessageW(f.window.hwnd(), WM_ACTIVATE, MAKEWPARAM(WA_INACTIVE, FALSE), 0);
  LOKA_VERIFY(GetFocus() != f.edit(3));
  ShowWindow(f.window.hwnd(), SW_MINIMIZE);
  LOKA_VERIFY(IsIconic(f.window.hwnd()));
  SetActiveWindow(other.hwnd());
  SetFocus(other.hwnd());
  // Windows itself refuses focus into a minimized window's child, so this
  // pins the outcome; the IsIconic guard alone is not what it discriminates.
  SendMessageW(f.window.hwnd(), WM_ACTIVATE, MAKEWPARAM(WA_CLICKACTIVE, FALSE), 0);
  LOKA_VERIFY(GetFocus() != f.edit(3));
  f.expect(3);
  ShowWindow(f.window.hwnd(), SW_RESTORE);
  SetActiveWindow(f.window.hwnd());

  verifyLifecycle(f, true);
}

namespace
{
  class CompletionApp : public Win32App
  {
  public:
    explicit CompletionApp(Window &window) : Win32App(0, GetModuleHandleW(0), SW_SHOW)
    {
      this->group_ = new AppComponentGroup(std::vector<AppComponent *>(1, &window));
      this->setActiveWindow(&window);
    }
    ~CompletionApp() { this->group_->build(); }
  };
  struct CompletionProbe
  {
    Fixture &fixture;
    unsigned step;
    explicit CompletionProbe(Fixture &f) : fixture(f), step(0) {}
    void tick()
    {
      if (this->step < 2)
        this->fixture.none();
      else
        this->fixture.expect(static_cast<unsigned short>(this->step - 1));
      if (this->step == 4)
      {
        PostQuitMessage(0);
        return;
      }
      ++this->step;
      LOKA_VERIFY(PostMessageW(GetFocus(), WM_KEYDOWN, VK_TAB, 0));
    }
  };
  CompletionProbe *completionProbe = 0;
  void CALLBACK completionTimer(HWND, UINT, UINT_PTR, DWORD)
  {
    completionProbe->tick();
  }
}

void testWin32FocusCompletion()
{
  for (unsigned mode = 0; mode != 2; ++mode)
  {
    Fixture f(mode != 0);
    if (!f.activate())
    {
      skipWithoutActivation("testWin32FocusCompletion",
                            "the message-loop Tab and completion checks",
                            "testWin32FocusReadAndRestore keeps the checks that need no active window.");
      return;
    }
    CompletionApp app(f.window);
    CompletionProbe probe(f);
    completionProbe = &probe;
    const UINT_PTR timer = SetTimer(0, 0, 40, &completionTimer);
    LOKA_VERIFY(timer);
    app.run(); // Real PeekMessage/IsDialogMessage/admission/completion/WaitMessage.
    KillTimer(0, timer);
    completionProbe = 0;
    f.window.setApp(0);
    LOKA_VERIFY(probe.step == 4);
    f.expect(3);
  }
}
