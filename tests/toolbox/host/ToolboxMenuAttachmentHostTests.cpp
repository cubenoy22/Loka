#include "ToolboxMenuHost.hpp"
#include "support/TestVerify.hpp"
#include "support/StandaloneMountTestSupport.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/MenuController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "Script.h"
#include <cstdio>
#include <cstring>

namespace toolbox_host
{
  std::vector<std::string> menuTitles, menuAppends, menuInserts, menuSets, disposedMenuItems;
  std::vector<MenuHandle> installedMenus;
  void (*afterMenuSet)() = 0;
  unsigned menuDraws = 0, menuClears = 0, menuDisposes = 0, menuValueWrites = 0;
}
long GetScriptManagerVariable(short) { return smRoman; }
using namespace loka::app;
using namespace loka::app::scene;
using namespace loka::core;
namespace
{
  void clearCalls()
  {
    toolbox_host::menuTitles.clear();
    toolbox_host::menuAppends.clear();
    toolbox_host::menuInserts.clear();
    toolbox_host::menuSets.clear();
    toolbox_host::menuDraws = toolbox_host::menuClears = toolbox_host::menuDisposes = toolbox_host::menuValueWrites = 0;
  }
  void noCalls()
  {
    LOKA_VERIFY(toolbox_host::menuTitles.empty());
    LOKA_VERIFY(toolbox_host::menuAppends.empty());
    LOKA_VERIFY(toolbox_host::menuInserts.empty());
    LOKA_VERIFY(toolbox_host::menuSets.empty());
    LOKA_VERIFY(toolbox_host::menuDraws == 0 && toolbox_host::menuClears == 0);
    LOKA_VERIFY(toolbox_host::menuDisposes == 0 && toolbox_host::menuValueWrites == 0);
  }
  MenuBarDefinition bar(const char *item = "Open")
  {
    MenuBarDefinition result;
    result << (Menu("File") << MenuItem(item));
    result << (Menu("Edit") << MenuItem("Copy"));
    return result;
  }
  class OpaqueMenu : public MenuBoundary
  {
  public:
    // This legacy fixture intentionally exercises MenuBoundary's state door;
    // it is the production contract N2b must keep until N3 removes that owner.
    OpaqueMenu() : checked_(this->dangerouslyUseState(false)) {}
    virtual void composeMenu(MenuComposition &c)
    {
      c << (Menu("View") << MenuItem("Actual").attr(MenuItemAttr().checked(this->checked_.get())));
    }
    void flip() { StateTrackerGuard guard(this->tracker()); this->checked_.set(true); }
  private:
    MutableState<bool> &checked_;
  };
  class Config : public AppConfigurable
  {
  public:
    Config() : AppConfigurable(0) {}
    virtual void compose(AppComposition &) {}
    virtual void composeMenu(MenuComposition &c) { c << this->menu; }
    OpaqueMenu menu;
  };
  struct Observer
  {
    Observer() : attachment(0), calls(0), detaches(0), disconnectInAction(false) {}
    ToolboxMenuAttachment *attachment;
    int calls, detaches;
    bool disconnectInAction;
  };
  Observer *observer = 0;
  class Root : public BoundaryNodeFor<Root>
  {
  public:
    explicit Root(const BoundaryPropsFor<Root> &props) : BoundaryNodeFor<Root>(props)
    { this->state(this->enabled_, true); }
    virtual void declareBindings(BindingToken &token) { token.action(this->clicked_, this, &Root::clicked); }
    virtual void composeNode(NodeComposition &c)
    {
      MenuBarDefinition offered;
      offered << (Menu("File") << MenuItem("Run").enabled(this->enabled_.state()).onClick(&this->clicked_));
      LOKA_VERIFY(c.menuBar(offered));
    }
    virtual void detachNode(NodeComposition &)
    {
      ++observer->detaches;
      LOKA_VERIFY(!observer->attachment->dispatch(128, 1));
    }
    void disable() { StateTrackerGuard guard(this->tracker()); this->enabled_.set(!this->enabled_.get()); }
  private:
    void clicked()
    {
      ++observer->calls;
      if (observer->disconnectInAction)
        observer->attachment->disconnect();
    }
    NodeState<bool> enabled_;
    EmitterState clicked_;
  };
  struct Mounted
  {
    Observer observation;
    loka::testing::StandaloneMountTestPlatformContext platform;
    ToolboxApp app;
    ToolboxWindow window;
    ToolboxScenePlatformController controller;
    Mounted() : window(&platform, props(), &app), controller(&window)
    {
      observer = &this->observation;
      this->observation.attachment = &this->app.menuAttachment();
      this->window.scene()->mount(&this->controller);
      LOKA_VERIFY(this->window.scene()->menuBar());
      LOKA_VERIFY(this->app.menuAttachment().project(this->window.scene()->menuBar(), this->window.scene(), false));
    }
    ~Mounted() { this->window.unmount(); }
    static WindowProps props()
    {
      WindowProps result;
      result.scene(new Scene(Boundary<Root>()));
      return result;
    }
    Root *root()
    {
      return static_cast<Root *>(loka::dsl::testing::SceneTestAccess::rootBoundary(*this->window.scene()));
    }
  };
}

void testToolboxMenuAttachmentProjectsOnceForEqualBar()
{
  ToolboxApp app;
  MenuBarDefinition first = bar(), equal = bar();
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(&first, 0, false));
  LOKA_VERIFY(toolbox_host::menuTitles.size() == 2 && toolbox_host::menuSets.size() == 2);
  clearCalls();
  LOKA_VERIFY(!app.menuAttachment().project(&equal, 0, false));
  noCalls();
  LOKA_VERIFY(!app.menuAttachment().project(&equal, 0, true));
  noCalls();
}
void testToolboxMenuAttachmentSeesOpaqueMenuItemChange()
{
  Config config;
  ToolboxApp app(&config);
  app.projectMenu(0, app.resolveForTest(), 0);
  LOKA_VERIFY(toolbox_host::menuSets.back() == "Actual");
  config.menu.flip();
  clearCalls();
  LOKA_VERIFY(app.flushMenuInvalidation());
  app.projectMenu(0, app.defaultMenuBar(), 0);
  LOKA_VERIFY(toolbox_host::menuSets.size() == 1 && toolbox_host::menuSets[0] == "Actual");
  LOKA_VERIFY(toolbox_host::menuTitles.empty());
  clearCalls();
  app.projectMenu(0, app.resolveForTest(), 0);
  noCalls();
}
void testToolboxMenuAttachmentPartialRebuildFromBaseline()
{
  ToolboxApp app;
  MenuBarDefinition first = bar(), changed = bar("Save");
  LOKA_VERIFY(app.menuAttachment().project(&first, 0, false));
  MenuHandle unchanged = toolbox_host::installedMenus[1];
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(&changed, 0, false));
  LOKA_VERIFY(toolbox_host::menuTitles.empty() && toolbox_host::menuDisposes == 0);
  LOKA_VERIFY(toolbox_host::menuSets.size() == 1 && toolbox_host::menuSets[0] == "Save");
  LOKA_VERIFY(toolbox_host::installedMenus[1] == unchanged && unchanged->items[0] == "Copy");
  changed << (Menu("Help") << MenuItem("Help"));
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(&changed, 0, false));
  LOKA_VERIFY(toolbox_host::menuDisposes == 2 && toolbox_host::menuTitles.size() == 3);
}
void testToolboxMenuAttachmentReleaseFromSourceDisconnects()
{
  Mounted f;
  Scene other((Boundary<Root>()));
  clearCalls();
  f.app.menuAttachment().releaseFrom(&other);
  LOKA_VERIFY(f.app.menuAttachment().dispatch(128, 1));
  LOKA_VERIFY(f.observation.calls == 1);
  const int before = f.app.redraws;
  f.root()->disable();
  LOKA_VERIFY(f.app.redraws == before + 1); // Positive control: live State reaches the thunk.
  clearCalls();
  f.app.menuAttachment().releaseFrom(f.window.scene());
  noCalls();
  LOKA_VERIFY(!f.app.menuAttachment().dispatch(128, 1));
  LOKA_VERIFY(toolbox_host::installedMenus.size() == 1);
  f.root()->disable();
  noCalls();
  MenuBarDefinition replacement = bar();
  LOKA_VERIFY(f.app.menuAttachment().project(&replacement, 0, false));
  LOKA_VERIFY(toolbox_host::menuDisposes == 1);
  f.app.menuAttachment().disconnect();
}
void testToolboxSceneDetachReleasesMenu()
{
  Mounted f;
  LOKA_VERIFY(f.app.menuAttachment().dispatch(128, 1));
  clearCalls();
  f.window.unmount();
  LOKA_VERIFY(f.observation.detaches == 1 && f.observation.calls == 1);
  LOKA_VERIFY(!f.app.menuAttachment().dispatch(128, 1));
  noCalls();
}
void testToolboxMenuDispatchAfterDisconnectIsDropped()
{
  Mounted f;
  LOKA_VERIFY(f.app.menuAttachment().dispatch(128, 1));
  LOKA_VERIFY(f.observation.calls == 1);
  f.app.menuAttachment().disconnect();
  LOKA_VERIFY(!f.app.menuAttachment().dispatch(128, 1));
  LOKA_VERIFY(f.observation.calls == 1);
}
void testToolboxMenuDispatchMayDisconnectWhileRunning()
{
  Mounted f;
  f.observation.disconnectInAction = true;
  LOKA_VERIFY(f.app.menuAttachment().dispatch(128, 1));
  LOKA_VERIFY(f.observation.calls == 1);
  LOKA_VERIFY(!f.app.menuAttachment().dispatch(128, 1));
}
void testMenuProjectionDiffValueCopiesOwnRows()
{
  MenuBarDefinition first = bar(), changed = bar("Save");
  MenuCompositionDiff copied;
  {
    MenuCompositionDiff original = MenuCompositionDiff::DiffProjection(&first, changed);
    copied = original;
    LOKA_VERIFY(copied.changedHead() != original.changedHead());
    MenuCompositionDiff copy(original);
    LOKA_VERIFY(copy.changedCount() == 1 && copy.changedHead() != original.changedHead());
  }
  LOKA_VERIFY(copied.valid && !copied.fullRebuild && copied.changedHead()->value == 0);
  LOKA_VERIFY(!first.equalsProjection(changed));
  LOKA_VERIFY(first.equalsProjection(first));
}
void testToolboxMenuAttachmentCloneRefusalClearsAppliedBaseline()
{
  struct RefreshApp : ToolboxApp
  {
    explicit RefreshApp(AppConfigurable *config) : ToolboxApp(config) {}
    void refresh() { LOKA_VERIFY(this->refreshDefaultMenuBar()); }
  };
  Config config;
  RefreshApp app(&config);
  app.projectMenu(0, app.resolveForTest(), 0);
  config.menu.flip();
  app.refresh(); // Legacy controller has captured and acknowledged the new bar.
  loka::app::testing::failMenuBarDefinitionClones(1);
  clearCalls();
  app.projectMenu(0, app.resolveForTest(), 0);
  LOKA_VERIFY(toolbox_host::menuSets.size() == 1 && toolbox_host::menuSets[0] == "Actual");
  LOKA_VERIFY(app.defaultMenuBar()->menuAt(0)->itemsHead()->isCheckedInitial());
  loka::app::testing::allowMenuBarDefinitionClones();
  clearCalls();
  app.projectMenu(0, app.resolveForTest(), 0);
  // Failed capture left no baseline, so even an equal offer must rebuild fully.
  LOKA_VERIFY(toolbox_host::menuTitles.size() == 1 && toolbox_host::menuDisposes == 1);
  clearCalls();
  app.projectMenu(0, app.resolveForTest(), 0);
  noCalls();
}

void testToolboxMenuDispatchKeepsActionAcrossReplacement()
{
  Config config;
  ToolboxApp app(&config);
  struct Replace
  {
    static void run(void *data)
    {
      ToolboxApp *app = static_cast<ToolboxApp *>(data);
      MenuBarDefinition replacement = bar("Replacement");
      LOKA_VERIFY(app->menuAttachment().project(&replacement, 0, false));
    }
  };
  EmitterState emitter;
  emitter.deferBind(&Replace::run, &app);
  MenuBarDefinition original;
  original << (Menu("File") << MenuItem("Rebuild").actionType(MENU_ACTION_REBUILD_MENU).onClick(&emitter));
  LOKA_VERIFY(app.menuAttachment().project(&original, 0, false));
  LOKA_VERIFY(!config.menuRefresh().hasPendingRequest());
  LOKA_VERIFY(app.menuAttachment().dispatch(128, 1));
  LOKA_VERIFY(config.menuRefresh().hasPendingRequest());
  emitter.deferUnbind(&Replace::run, &app);
}
void testToolboxMenuAttachmentSwitchesSourceAndClearsNullBar()
{
  ToolboxApp app;
  Scene first((Boundary<Root>())), second((Boundary<Root>()));
  MenuBarDefinition offered = bar();
  LOKA_VERIFY(app.menuAttachment().project(&offered, &first, false));
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(&offered, &second, false));
  LOKA_VERIFY(toolbox_host::menuDisposes == 2 && toolbox_host::menuTitles.size() == 2);
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(0, 0, false));
  LOKA_VERIFY(toolbox_host::menuDisposes == 2 && toolbox_host::menuClears == 1 && toolbox_host::menuDraws == 1);
  LOKA_VERIFY(toolbox_host::installedMenus.empty());
  // Completion re-offers every turn: an absent bar over an empty projection
  // must neither clear nor draw again (codex review of N2a).
  clearCalls();
  const int redrawsBefore = app.redraws;
  LOKA_VERIFY(!app.menuAttachment().project(0, 0, false));
  LOKA_VERIFY(toolbox_host::menuClears == 0 && toolbox_host::menuDraws == 0 && app.redraws == redrawsBefore);
}
void testToolboxMenuAttachmentMissingEntryFallsBackWithPreparedClone()
{
  ToolboxApp app;
  MenuBarDefinition before, after;
  before << (Menu("File") << MenuItem("Open")) << Menu("Empty");
  after << (Menu("File") << MenuItem("Save")) << (Menu("Empty") << MenuItem("New"));
  LOKA_VERIFY(app.menuAttachment().project(&before, 0, false));
  clearCalls();
  // Refuse any clone attempted after the first native item mutation. The
  // partial-to-full fallback must reuse the snapshot prepared before it.
  toolbox_host::afterMenuSet = &loka::app::testing::failNextMenuBarDefinitionClone;
  LOKA_VERIFY(app.menuAttachment().project(&after, 0, false));
  toolbox_host::afterMenuSet = 0;
  loka::app::testing::allowMenuBarDefinitionClones();
  LOKA_VERIFY(toolbox_host::menuTitles.size() == 2 && toolbox_host::menuDraws == 1);
  clearCalls();
  LOKA_VERIFY(!app.menuAttachment().project(&after, 0, false));
  noCalls();
}
namespace
{
  class ShutdownApp : public ToolboxApp
  {
  public:
    void own(Window *window)
    {
      if (!this->group_)
        this->group_ = new AppComponentGroup(std::vector<AppComponent *>());
      this->group_->adopt(window);
    }
    void retireForTest() { this->retireComponents(); }
  };
  class ShutdownWindow : public ToolboxWindow
  {
  public:
    ShutdownWindow(PlatformContext *platform, ToolboxApp *app)
        : ToolboxWindow(platform, Mounted::props(), app), controller_(this)
    {
      this->scene()->mount(&this->controller_);
    }
    virtual ~ShutdownWindow()
    {
      // A positive native-handle witness makes the bad destructor order red
      // before invoking the production release door on a dead attachment.
      LOKA_VERIFY(toolbox_host::menuDisposes == 0);
      this->unmount();
      LOKA_VERIFY(observer->detaches == 1);
    }
  private:
    ToolboxScenePlatformController controller_;
  };
  void shutdownMenu(bool queued, bool retireTwice)
  {
    Observer observation;
    observer = &observation;
    loka::testing::StandaloneMountTestPlatformContext platform;
    {
      ShutdownApp app;
      observation.attachment = &app.menuAttachment();
      ShutdownWindow *window = new ShutdownWindow(&platform, &app);
      app.own(window);
      LOKA_VERIFY(app.menuAttachment().project(window->scene()->menuBar(), window->scene(), false));
      LOKA_VERIFY(app.menuAttachment().dispatch(128, 1));
      LOKA_VERIFY(observation.calls == 1);
      if (queued)
        app.requestWindowClose(window);
      clearCalls();
      if (retireTwice)
      {
        app.retireForTest();
        LOKA_VERIFY(observation.detaches == 1);
        app.retireForTest();
        LOKA_VERIFY(observation.detaches == 1);
      }
    }
    LOKA_VERIFY(observation.detaches == 1);
    LOKA_VERIFY(toolbox_host::menuDisposes == 1);
    LOKA_VERIFY(toolbox_host::installedMenus.empty());
  }
}
void testToolboxAppShutdownReleasesMenuBeforeAttachmentDestruction()
{
  shutdownMenu(false, false);
}
void testToolboxAppShutdownDrainsPendingCloseBeforeAttachmentDestruction()
{
  shutdownMenu(true, false);
}
void testAppRetireComponentsIsIdempotent()
{
  shutdownMenu(false, true);
  shutdownMenu(true, true);
}
void testAppDestructorRetiresComponentsWithoutRailOwner()
{
  struct CountedComponent : AppComponent
  {
    explicit CountedComponent(int &deletions) : deletions_(deletions) {}
    virtual ~CountedComponent() { ++this->deletions_; }
    int &deletions_;
  };
  struct BaseOnlyApp : App
  {
    explicit BaseOnlyApp(AppComponent *component) : App(0)
    {
      this->group_ = new AppComponentGroup(std::vector<AppComponent *>());
      this->group_->adopt(component);
    }
    virtual void quit() {}
  };
  int deletions = 0;
  {
    BaseOnlyApp app(new CountedComponent(deletions));
  }
  LOKA_VERIFY(deletions == 1);
}
void testToolboxMenuDoorFiltersRowsAndForwardsScene()
{
  Mounted f;
  ToolboxWindow inactive(&f.platform, WindowProps(), &f.app);
  MenuBarDefinition other = bar("Other");
  f.app.setActiveWindow(&f.window);
  clearCalls();
  f.app.projectMenu(&inactive, &other, 0);
  f.app.projectMenu(0, &other, 0);
  noCalls();
  // Equal Scene offer proves the door retained the source identity as well as
  // the contents; dropping the source would force a native rebuild here.
  f.app.projectMenu(&f.window, f.window.scene()->menuBar(), f.window.scene());
  noCalls();
  f.app.setActiveWindow(0);
  f.app.projectMenu(0, &other, 0);
  LOKA_VERIFY(toolbox_host::menuDraws == 1);
  f.app.menuAttachment().disconnect();
}
void testToolboxMenuProjectionDefersBackgroundDraws()
{
  ToolboxApp app;
  MenuBarDefinition first = bar(), partial = bar("Save");
  app.phase(ACTIVATION_BACKGROUND);
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(&first, 0, false));
  LOKA_VERIFY(toolbox_host::menuTitles.size() == 2);
  LOKA_VERIFY(toolbox_host::menuDraws == 0 && app.drawOwed());
  app.phase(ACTIVATION_FOREGROUND);
  LOKA_VERIFY(toolbox_host::menuDraws == 1 && !app.drawOwed());
  app.phase(ACTIVATION_FOREGROUND);
  LOKA_VERIFY(toolbox_host::menuDraws == 1);
  app.phase(ACTIVATION_BACKGROUND);
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(&partial, 0, false));
  LOKA_VERIFY(toolbox_host::menuTitles.empty() && toolbox_host::menuSets.size() == 1);
  LOKA_VERIFY(toolbox_host::menuDraws == 0 && app.drawOwed());
  app.phase(ACTIVATION_FOREGROUND);
  LOKA_VERIFY(toolbox_host::menuDraws == 1 && !app.drawOwed());
  app.phase(ACTIVATION_BACKGROUND);
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(0, 0, false));
  LOKA_VERIFY(toolbox_host::installedMenus.empty());
  LOKA_VERIFY(toolbox_host::menuDraws == 0 && app.drawOwed());
  app.phase(ACTIVATION_FOREGROUND);
  LOKA_VERIFY(toolbox_host::menuDraws == 1 && !app.drawOwed());
  clearCalls();
  LOKA_VERIFY(app.menuAttachment().project(&first, 0, false));
  LOKA_VERIFY(toolbox_host::menuDraws == 1 && !app.drawOwed());
}
int main(int argc, char **argv)
{
  struct Test { const char *name; void (*run)(); };
#define PIN(name) {#name, &name}
  const Test tests[] = {
    PIN(testToolboxMenuDoorFiltersRowsAndForwardsScene),
    PIN(testToolboxMenuProjectionDefersBackgroundDraws),
    PIN(testToolboxMenuAttachmentProjectsOnceForEqualBar),
    PIN(testToolboxMenuAttachmentSeesOpaqueMenuItemChange),
    PIN(testToolboxMenuAttachmentPartialRebuildFromBaseline),
    PIN(testToolboxMenuAttachmentReleaseFromSourceDisconnects),
    PIN(testToolboxSceneDetachReleasesMenu),
    PIN(testToolboxMenuDispatchAfterDisconnectIsDropped),
    PIN(testToolboxMenuDispatchMayDisconnectWhileRunning),
    PIN(testMenuProjectionDiffValueCopiesOwnRows),
    PIN(testToolboxMenuAttachmentCloneRefusalClearsAppliedBaseline),
    PIN(testToolboxMenuDispatchKeepsActionAcrossReplacement),
    PIN(testToolboxMenuAttachmentSwitchesSourceAndClearsNullBar),
    PIN(testToolboxMenuAttachmentMissingEntryFallsBackWithPreparedClone),
    PIN(testToolboxAppShutdownReleasesMenuBeforeAttachmentDestruction),
    PIN(testToolboxAppShutdownDrainsPendingCloseBeforeAttachmentDestruction),
    PIN(testAppRetireComponentsIsIdempotent),
    PIN(testAppDestructorRetiresComponentsWithoutRailOwner)
  };
#undef PIN
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    if (argc == 1 || std::strcmp(argv[1], tests[i].name) == 0)
    {
      tests[i].run();
      std::printf("PASS %s\n", tests[i].name);
    }
}
