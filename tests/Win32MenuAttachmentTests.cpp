#include "Win32MenuAttachmentTests.hpp"
#include <cstdio>
#include "Win32App.hpp"
#include "Win32Window.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/Win32DisplayScale.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;

  class MenuApp : public Win32App
  {
  public:
    MenuApp() : Win32App(0, GetModuleHandleW(NULL), SW_SHOW) {}
    virtual ~MenuApp() {}
    virtual void applyMenuBar(Window *window)
    {
      std::fprintf(stderr, "DIAG applyMenuBar window=%p active=%p\n", (void *)window, (void *)this->activeWindow());
      Win32App::applyMenuBar(window);
    }
    void own(Window *window)
    {
      if (!this->group_)
        this->group_ = new AppComponentGroup(std::vector<AppComponent *>());
      this->group_->adopt(window);
    }
  };

  void show(Win32Window &window, bool visible)
  {
    {
      StateTrackerGuard guard(window.getTracker());
      window.visibilityState().set(visible, true);
    }
    WindowAdmissionTestApp admission(window);
    admission.flush();
  }

  WindowProps props()
  {
    WindowProps result;
    result.frame(60, 60, 320, 200).visible(false);
    return result;
  }

  MenuBarDefinition bar(const char *title, EmitterState *emitter = 0)
  {
    MenuBarDefinition result;
    result << (Menu("File") << MenuItem(title).onClick(emitter));
    return result;
  }

  UINT firstCommand(Win32Window &window)
  {
    HMENU menu = GetMenu(window.hwnd());
    LOKA_VERIFY(menu);
    HMENU popup = GetSubMenu(menu, 0);
    LOKA_VERIFY(popup);
    const UINT id = GetMenuItemID(popup, 0);
    LOKA_VERIFY(id != static_cast<UINT>(-1));
    return id;
  }

  void verifyFrame(Win32Window &window, const Frame &before)
  {
    Frame after;
    LOKA_VERIFY(window.queryNativeContentFrame(after));
    LOKA_VERIFY(after == before);
    RECT client;
    LOKA_VERIFY(GetClientRect(window.hwnd(), &client));
    const loka::win32::Win32DisplayScale scale(
        loka::win32::Win32DisplayScale::forWindow(window.hwnd()).dpi(),
        loka::win32::DefaultRailMetrics());
    LOKA_VERIFY(client.right - client.left == scale.clientLengthToNative(before.width).px);
    LOKA_VERIFY(client.bottom - client.top == scale.clientLengthToNative(before.height).px);
  }

  struct Observation
  {
    Observation() : window(0), ownerAtShutdown(0), command(0), calls(0), detaches(0) {}
    Win32Window *window;
    App *ownerAtShutdown;
    UINT command;
    int calls;
    int detaches;
  };
  Observation *composingObservation = 0;

  class MenuRoot : public BoundaryNodeFor<MenuRoot>
  {
  public:
    explicit MenuRoot(const BoundaryPropsFor<MenuRoot> &p)
        : BoundaryNodeFor<MenuRoot>(p), observation_(*composingObservation)
    {
      this->state(this->enabled_, true);
    }
    virtual void declareBindings(BindingToken &token)
    {
      token.action(this->clicked_, this, &MenuRoot::clicked);
    }
    virtual void composeNode(NodeComposition &c)
    {
      MenuBarDefinition offered;
      offered << (Menu("File") << MenuItem("Run").enabled(this->enabled_.state()).onClick(&this->clicked_));
      LOKA_VERIFY(c.menuBar(offered));
    }
    virtual void detachNode(NodeComposition &)
    {
      ++this->observation_.detaches;
      Win32Window &window = *this->observation_.window;
      LOKA_VERIFY(GetMenu(window.hwnd()) != NULL);
      LOKA_VERIFY(!window.menuAttachment().dispatch(this->observation_.command));
      if (this->observation_.ownerAtShutdown)
      {
        // Virtual dispatch through the base borrow witnesses that Win32App is
        // still alive. Without its retireComponents(), App's default returns
        // false here. No active window means this continuation projects nothing.
        LOKA_VERIFY(this->observation_.ownerAtShutdown->handleMenuCommand(-1, &window));
      }
    }
    void enabled(bool value)
    {
      StateTrackerGuard guard(this->tracker());
      this->enabled_.set(value);
    }
  private:
    void clicked() { ++this->observation_.calls; }
    Observation &observation_;
    NodeState<bool> enabled_;
    EmitterState clicked_;
  };

  WindowProps sceneProps(Observation &observation)
  {
    composingObservation = &observation;
    WindowProps result = props();
    result.scene(new Scene(Boundary<MenuRoot>()));
    return result;
  }

  struct Mounted
  {
    Observation observation;
    NullPlatformContext context;
    MenuApp app;
    Win32Window window;
    Mounted() : window(&this->context, sceneProps(this->observation))
    {
      this->observation.window = &this->window;
      this->window.setApp(&this->app);
      show(this->window, true);
      LOKA_VERIFY(this->window.scene()->menuBar());
      LOKA_VERIFY(this->window.menuAttachment().project(this->window.scene()->menuBar(), this->window.scene()));
      this->observation.command = firstCommand(this->window);
    }
    ~Mounted() { show(this->window, false); }
    MenuRoot &root()
    {
      return *static_cast<MenuRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(*this->window.scene()));
    }
  };

  void count(void *data) { ++*static_cast<int *>(data); }
}

void testWin32MenuAttachmentProjectsOnceForEqualBar()
{
  NullPlatformContext context;
  Win32Window window(&context, props());
  show(window, true);
  Frame before;
  LOKA_VERIFY(window.queryNativeContentFrame(before));
  MenuBarDefinition first = bar("Open"), equal = bar("Open"), changed = bar("Save");
  LOKA_VERIFY(window.menuAttachment().project(&first, 0));
  HMENU installed = GetMenu(window.hwnd());
  LOKA_VERIFY(installed && IsMenu(installed));
  verifyFrame(window, before);
  LOKA_VERIFY(!window.menuAttachment().project(&equal, 0));
  LOKA_VERIFY(GetMenu(window.hwnd()) == installed);
  LOKA_VERIFY(window.menuAttachment().project(&changed, 0));
  LOKA_VERIFY(GetMenu(window.hwnd()) != installed);
  LOKA_VERIFY(!IsMenu(installed));
  verifyFrame(window, before);
  LOKA_VERIFY(window.menuAttachment().project(0, 0));
  LOKA_VERIFY(!GetMenu(window.hwnd()));
  verifyFrame(window, before);
  // Equal offers after native recreation must rebuild, never reuse a dead HMENU.
  LOKA_VERIFY(window.menuAttachment().project(&changed, 0));
  show(window, false);
  show(window, true);
  LOKA_VERIFY(window.menuAttachment().project(&changed, 0));
  LOKA_VERIFY(GetMenu(window.hwnd()) && IsMenu(GetMenu(window.hwnd())));
  show(window, false);
}

void testWin32MenuAttachmentReleaseFromSourceDisconnects()
{
  Mounted f;
  Scene other((Boundary<MenuRoot>()));
  HMENU installed = GetMenu(f.window.hwnd());
  const UINT id = f.observation.command;
  HMENU popup = GetSubMenu(installed, 0);
  LOKA_VERIFY(GetMenuState(popup, id, MF_BYCOMMAND) == MF_ENABLED);
  f.window.menuAttachment().releaseFrom(&other);
  LOKA_VERIFY(f.window.menuAttachment().dispatch(id));
  LOKA_VERIFY(f.observation.calls == 1);
  f.root().enabled(false);
  const UINT disabled = GetMenuState(popup, id, MF_BYCOMMAND);
  LOKA_VERIFY(disabled != static_cast<UINT>(-1) && (disabled & MF_GRAYED));
  f.window.menuAttachment().releaseFrom(f.window.scene());
  LOKA_VERIFY(!f.window.menuAttachment().dispatch(id));
  f.root().enabled(true);
  LOKA_VERIFY(GetMenuState(popup, id, MF_BYCOMMAND) == disabled);
  LOKA_VERIFY(GetMenu(f.window.hwnd()) == installed && IsMenu(installed));
  LOKA_VERIFY(f.observation.calls == 1);
}

void testWin32SceneDetachReleasesMenu()
{
  Mounted f;
  HMENU installed = GetMenu(f.window.hwnd());
  LOKA_VERIFY(f.window.menuAttachment().dispatch(f.observation.command));
  LOKA_VERIFY(f.observation.calls == 1);
  loka::dsl::testing::SceneTestAccess::unmount(*f.window.scene());
  LOKA_VERIFY(f.observation.detaches == 1);
  LOKA_VERIFY(!f.window.menuAttachment().dispatch(f.observation.command));
  LOKA_VERIFY(GetMenu(f.window.hwnd()) == installed);
  {
    Mounted native;
    native.window.setApp(0); // External DestroyWindow must not queue this stack Window.
    const HWND hwnd = native.window.hwnd();
    const HMENU menu = GetMenu(hwnd);
    LOKA_VERIFY(DestroyWindow(hwnd));
    LOKA_VERIFY(native.observation.detaches == 1);
    LOKA_VERIFY(!native.window.menuAttachment().dispatch(native.observation.command));
    LOKA_VERIFY(!native.window.hwnd() && !IsMenu(menu));
  }
}

void testWin32TwoWindowsOwnTheirMenus()
{
  NullPlatformContext context;
  MenuApp app;
  Win32Window a(&context, props()), b(&context, props());
  int callsA = 0, callsB = 0;
  EmitterState emitterA, emitterB;
  emitterA.deferBind(&count, &callsA);
  emitterB.deferBind(&count, &callsB);
  a.setApp(&app);
  b.setApp(&app);
  show(a, true);
  show(b, true);
  MenuBarDefinition barA = bar("A", &emitterA), barB = bar("B", &emitterB);
  // Exercise the production apply path: B's projection used to detach A.
  std::fprintf(stderr, "DIAG a=%p b=%p\n", (void *)&a, (void *)&b);
  app.setActiveWindow(&a);
  app.setDefaultMenuBar(&barA);
  HMENU menuA = GetMenu(a.hwnd());
  LOKA_VERIFY(menuA);
  std::fprintf(stderr, "DIAG after barA: GetMenu(a)=%p menuA=%p IsMenu=%d GetMenu(b)=%p\n", (void *)GetMenu(a.hwnd()), (void *)menuA, IsMenu(menuA) ? 1 : 0, (void *)GetMenu(b.hwnd()));
  app.setActiveWindow(&b);
  std::fprintf(stderr, "DIAG after active b: GetMenu(a)=%p IsMenu=%d GetMenu(b)=%p\n", (void *)GetMenu(a.hwnd()), IsMenu(menuA) ? 1 : 0, (void *)GetMenu(b.hwnd()));
  app.setDefaultMenuBar(&barB);
  std::fprintf(stderr, "DIAG after barB: GetMenu(a)=%p IsMenu=%d GetMenu(b)=%p\n", (void *)GetMenu(a.hwnd()), IsMenu(menuA) ? 1 : 0, (void *)GetMenu(b.hwnd()));
  HMENU menuB = GetMenu(b.hwnd());
  LOKA_VERIFY(menuB && menuB != menuA);
  LOKA_VERIFY(GetMenu(a.hwnd()) == menuA && IsMenu(menuA));
  const UINT idA = firstCommand(a), idB = firstCommand(b);
  LOKA_VERIFY(idA == idB && "command ids are local to the receiving HWND");
  LOKA_VERIFY(PostMessageW(b.hwnd(), WM_COMMAND, MAKEWPARAM(idB, 0), 0));
  MSG message;
  LOKA_VERIFY(PeekMessageW(&message, b.hwnd(), WM_COMMAND, WM_COMMAND, PM_REMOVE));
  DispatchMessageW(&message);
  LOKA_VERIFY(callsA == 0 && callsB == 1);
  SendMessageW(a.hwnd(), WM_COMMAND, MAKEWPARAM(idA, 0), 0);
  LOKA_VERIFY(callsA == 1 && callsB == 1);
  show(b, false);
  LOKA_VERIFY(GetMenu(a.hwnd()) == menuA && IsMenu(menuA));
  show(a, false);
  emitterA.deferUnbind(&count, &callsA);
  emitterB.deferUnbind(&count, &callsB);
}

void testWin32AppShutdownReleasesMenuBeforeAttachmentDestruction()
{
  Observation observation;
  NullPlatformContext context;
  HMENU installed = NULL;
  HWND hwnd = NULL;
  {
    MenuApp app;
    Win32Window *window = new Win32Window(&context, sceneProps(observation));
    observation.window = window;
    observation.ownerAtShutdown = &app;
    window->setApp(&app);
    show(*window, true);
    app.own(window);
    LOKA_VERIFY(window->menuAttachment().project(window->scene()->menuBar(), window->scene()));
    observation.command = firstCommand(*window);
    LOKA_VERIFY(window->menuAttachment().dispatch(observation.command));
    LOKA_VERIFY(observation.calls == 1);
    installed = GetMenu(window->hwnd());
    hwnd = window->hwnd();
    app.setActiveWindow(0);
  }
  LOKA_VERIFY(observation.detaches == 1);
  LOKA_VERIFY(!IsMenu(installed));
  LOKA_VERIFY(!IsWindow(hwnd));
}
