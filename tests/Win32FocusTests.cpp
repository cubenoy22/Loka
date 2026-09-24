#include "Win32FocusTests.hpp"
#include "Win32App.hpp"
#include "Win32Window.hpp"
#include "Win32ScenePlatformController.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32TextEditorContext.hpp"
#include "context/Win32FocusParticipant.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "support/Headless.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/TestVerify.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  typedef Focused<unsigned short> Fact;
  struct Facts : HeadlessStateOwner
  {
    Reported<Fact> focus;
    Facts() { StateBatchBase::CreateImmediateState(this, this->focus, Fact::none()); }
  };
  Facts *composingFacts = 0;
  class FocusRoot : public BoundaryNodeFor<FocusRoot>
  {
  public:
    explicit FocusRoot(const BoundaryPropsFor<FocusRoot> &p) : BoundaryNodeFor<FocusRoot>(p) {}
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Button());
      c.declare(EditText(EditTextProps().focusedAs(composingFacts->focus, static_cast<unsigned short>(1))));
      c.declare(EditText(EditTextProps().focusedAs(composingFacts->focus, static_cast<unsigned short>(2))));
      c.declare(TextEditor(TextEditorProps().focusedAs(composingFacts->focus, static_cast<unsigned short>(3))));
    }
  };
  struct Fixture
  {
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
      SetActiveWindow(this->window.hwnd());
      SetFocus(this->window.hwnd());
      LOKA_VERIFY(GetActiveWindow() == this->window.hwnd());
    }
    Node *field(unsigned index)
    {
      Node *node = loka::dsl::testing::SceneTestAccess::rootBoundary(*this->window.scene())->childrenHead();
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
    void none() { LOKA_VERIFY(!(this->facts.focus.state()->get() != Fact::none())); }
    void expect(unsigned short key) { LOKA_VERIFY(this->facts.focus.state()->get().is(key)); }
  };

  void tab(HWND root)
  {
    MSG msg = {};
    msg.hwnd = GetFocus();
    msg.message = WM_KEYDOWN;
    msg.wParam = VK_TAB;
    LOKA_VERIFY(IsDialogMessageW(root, &msg));
  }
}

void testWin32FocusReadAndRestore()
{
  Fixture f;
  NodeContext *answer = f.field(1)->getContext();
  LOKA_VERIFY(f.rail().readNativeFocus(answer) && !answer);
  f.app.reconcileFocus();
  f.none();
  // Negative control: even a marked root is not a descendant participant.
  LOKA_VERIFY(Win32FocusParticipant::attach(f.window.hwnd(), f.field(1)->getContext()));
  LOKA_VERIFY(f.rail().readNativeFocus(answer) && !answer);
  Win32FocusParticipant::detach(f.window.hwnd());
  tab(f.window.hwnd());
  LOKA_VERIFY(GetFocus() != f.window.hwnd());
  LOKA_VERIFY(!Win32FocusParticipant::read(GetFocus()));
  f.app.reconcileFocus();
  f.none(); // Button has userdata, but no participant atom.
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
    LOKA_VERIFY(!f.rail().readNativeFocus(answer));
    f.app.reconcileFocus();
    f.expect(key);
    SetActiveWindow(f.window.hwnd());
    LOKA_VERIFY(GetFocus() == f.edit(key));
    f.app.reconcileFocus();
    f.expect(key);
  }
  // The WM_ACTIVATE minimized bit must suppress restore even before IsIconic
  // catches up. A synthetic message isolates that guard from OS focus policy.
  SetFocus(f.window.hwnd());
  SendMessageW(f.window.hwnd(), WM_ACTIVATE, MAKEWPARAM(WA_ACTIVE, TRUE), 0);
  LOKA_VERIFY(GetFocus() != f.edit(3));
  ShowWindow(f.window.hwnd(), SW_MINIMIZE);
  LOKA_VERIFY(IsIconic(f.window.hwnd()));
  SetActiveWindow(other.hwnd());
  SetFocus(other.hwnd());
  SendMessageW(f.window.hwnd(), WM_ACTIVATE, MAKEWPARAM(WA_CLICKACTIVE, FALSE), 0);
  LOKA_VERIFY(GetFocus() != f.edit(3));
  f.expect(3);
  ShowWindow(f.window.hwnd(), SW_RESTORE);
  SetActiveWindow(f.window.hwnd());

  for (unsigned short key = 1; key <= 3; ++key)
  {
    Node *node = f.field(key);
    const HWND retired = f.edit(key);
    SetFocus(retired);
    f.app.reconcileFocus();
    f.expect(key);
    NotifySubtreeNodeDetached(node);
    LifecycleFactTestAccess::DeliverFacts(node);
    LOKA_VERIFY(!Win32FocusParticipant::read(retired));
    LOKA_VERIFY(!IsWindowVisible(retired));
    SetFocus(retired); // A hidden HWND must not provide a read source either.
    LOKA_VERIFY(f.rail().readNativeFocus(answer) && !answer);
    f.app.reconcileFocus();
    f.none();
    NotifySubtreeNodeAttached(node);
    LifecycleFactTestAccess::DeliverFacts(node);
    LOKA_VERIFY(Win32FocusParticipant::read(retired) == node->getContext());
    SetFocus(retired);
    f.app.reconcileFocus();
    f.expect(key);
    LifecycleFactTestAccess::MarkSubtreeRetired(node);
    LifecycleFactTestAccess::DeliverFacts(node);
    LOKA_VERIFY(IsWindow(retired)); // Native destruction is deferred.
    LOKA_VERIFY(!Win32FocusParticipant::read(retired));
    LOKA_VERIFY(f.rail().readNativeFocus(answer) && !answer);
    f.none();
  }
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
