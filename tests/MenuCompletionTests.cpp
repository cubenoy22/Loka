#include "MenuCompletionTests.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/MenuComposition.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "platform/null/NullApp.hpp"
#include "support/TestVerify.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "core/Operation.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  struct Offer
  {
    Offer(Window *w, const MenuBarDefinition *b, const Scene *s)
        : window(w), bar(b ? *b : MenuBarDefinition()), source(s), hasBar(b != 0) {}
    Window *window;
    MenuBarDefinition bar;
    const Scene *source;
    bool hasBar;
  };
  class RecordingApp : public NullApp
  {
  public:
    explicit RecordingApp(AppConfigurable *config = 0) : NullApp(config) {}
    void own(Window *w)
    {
      if (!this->group_)
        this->group_ = new AppComponentGroup(std::vector<AppComponent *>());
      this->group_->adopt(w);
    }
    using App::admitAndApplyWindows;
    using App::reclaimWindows;
    using App::clearMenuDiff;
    void operationLoop()
    {
      RunWindowAdmissionOperation(*this);
    }
    std::vector<Offer> offers;
  protected:
    virtual void projectMenu(Window *w, const MenuBarDefinition *bar, const Scene *source)
    {
      this->offers.push_back(Offer(w, bar, source));
    }
  };
  MenuBarDefinition defaultBar()
  {
    MenuBarDefinition bar;
    bar << (Menu("File") << MenuItem("Open")) << (Menu("Help") << MenuItem("Help"));
    return bar;
  }
  template <int Kind> class MenuRoot : public BoundaryNodeFor<MenuRoot<Kind> >
  {
  public:
    explicit MenuRoot(const BoundaryPropsFor<MenuRoot<Kind> > &props)
        : BoundaryNodeFor<MenuRoot<Kind> >(props) {}
    virtual void composeNode(NodeComposition &c)
    {
      MenuBarDefinition bar;
      switch (Kind)
      {
      case 0:
        return;
      case 1:
        bar << (Menu("View") << MenuItem("Zoom"));
        break;
      default:
        bar << (Menu("File") << MenuItem("Save"));
        break;
      }
      LOKA_VERIFY(c.menuBar(bar));
    }
  };
  template <int Kind> NullWindow *addWindow(RecordingApp &app, NullPlatformContext &platform)
  {
    WindowProps props;
    props.scene(new Scene(Boundary<MenuRoot<Kind> >()));
    NullWindow *window = new NullWindow(&platform, props);
    app.own(window);
    return window;
  }
  class RefreshMenu : public MenuBoundary
  {
  public:
    // Legacy MenuBoundary owns its tracked input until N3; this pin preserves
    // the refresh path while removing only its synchronous apply callback.
    RefreshMenu() : changed_(this->dangerouslyUseState(false)) {}
    virtual void composeMenu(MenuComposition &c)
    {
      c << (Menu("File") << MenuItem(this->changed_.get() ? "After" : "Before"));
    }
    void change() { StateTrackerGuard guard(this->tracker()); this->changed_.set(true); }
  private:
    MutableState<bool> &changed_;
  };
  class RefreshConfig : public AppConfigurable
  {
  public:
    RefreshConfig() : AppConfigurable(0), composes(0) {}
    virtual void compose(AppComposition &) {}
    virtual void composeMenu(MenuComposition &c) { ++this->composes; c << this->menu; }
    RefreshMenu menu;
    int composes;
  };

}
void testMenuProjectedAtCompletionNotOnActivation()
{
  NullPlatformContext platform;
  RecordingApp app;
  MenuBarDefinition bar = defaultBar();
  app.setDefaultMenuBar(&bar);
  NullWindow *window = new NullWindow(&platform, WindowProps());
  app.own(window);
  app.offers.clear();
  Operation turn;
  app.setActiveWindow(window);
  LOKA_VERIFY(app.offers.empty());
  turn.settle();
  app.admitAndApplyWindows();
  LOKA_VERIFY(app.offers.size() == 1);
  LOKA_VERIFY(app.offers[0].window == window && app.offers[0].bar.equalsProjection(bar));
  turn.close();
  app.reclaimWindows();
}
void testLastWindowCloseProjectsDefaultInSameTurn()
{
  NullPlatformContext platform;
  RecordingApp app;
  MenuBarDefinition bar = defaultBar();
  app.setDefaultMenuBar(&bar);
  NullWindow *window = new NullWindow(&platform, WindowProps());
  app.own(window);
  app.setActiveWindow(window);
  app.offers.clear();
  Operation turn;
  app.requestWindowClose(window);
  LOKA_VERIFY(app.offers.empty());
  turn.settle();
  app.admitAndApplyWindows();
  LOKA_VERIFY(app.offers.size() == 1);
  LOKA_VERIFY(app.offers[0].window == 0 && app.offers[0].source == 0);
  LOKA_VERIFY(app.offers[0].bar.equalsProjection(bar));
  turn.close();
  app.reclaimWindows();
}

void testMenuSourceMergesDefaultAndActiveScene()
{
  NullPlatformContext platform;
  RecordingApp app;
  const MenuBarDefinition base = defaultBar();
  app.setDefaultMenuBar(&base);
  NullWindow *a = addWindow<1>(app, platform);
  NullWindow *b = addWindow<2>(app, platform);
  NullWindow *plain = addWindow<0>(app, platform);
  app.setActiveWindow(a);
  app.operationLoop();
  LOKA_VERIFY(app.offers.size() == 6);
  MenuBarDefinition expectedA = defaultBar();
  expectedA << (Menu("View") << MenuItem("Zoom"));
  MenuBarDefinition expectedB;
  expectedB << (Menu("File") << MenuItem("Save")) << (Menu("Help") << MenuItem("Help"));
  LOKA_VERIFY(app.offers[0].window == a && app.offers[0].source == a->scene());
  LOKA_VERIFY(app.offers[0].bar.equalsProjection(expectedA));
  LOKA_VERIFY(app.offers[1].window == b && app.offers[1].source == b->scene());
  LOKA_VERIFY(app.offers[1].bar.equalsProjection(expectedB));
  LOKA_VERIFY(app.offers[2].window == plain && app.offers[2].source == 0);
  LOKA_VERIFY(app.offers[2].bar.equalsProjection(base));
}
void testMergeMenuBarsReplacesInPlaceAndAppendsInOrder()
{
  MenuBarDefinition base = defaultBar(), overlay;
  overlay << (Menu("View") << MenuItem("Zoom")) << (Menu("File") << MenuItem("Save"))
          << (Menu("Tools") << MenuItem("Run"));
  MenuBarDefinition expected;
  expected << (Menu("File") << MenuItem("Save")) << (Menu("Help") << MenuItem("Help"))
           << (Menu("View") << MenuItem("Zoom")) << (Menu("Tools") << MenuItem("Run"));
  OwnedDef<MenuBarDefinition> merged(MergeMenuBars(&base, &overlay));
  LOKA_VERIFY(merged.isSet() && merged->equalsProjection(expected));
  LOKA_VERIFY(base.equalsProjection(defaultBar()));
  OwnedDef<MenuBarDefinition> baseOnly(MergeMenuBars(&base, 0));
  OwnedDef<MenuBarDefinition> overlayOnly(MergeMenuBars(0, &overlay));
  LOKA_VERIFY(baseOnly.isSet() && baseOnly->equalsProjection(base));
  LOKA_VERIFY(overlayOnly.isSet() && overlayOnly->equalsProjection(overlay));
  OwnedDef<MenuBarDefinition> absent(MergeMenuBars(0, 0));
  LOKA_VERIFY(!absent.isSet());
  loka::app::testing::failMenuBarDefinitionClones(2);
  OwnedDef<MenuBarDefinition> refusedBase(MergeMenuBars(&base, &overlay));
  OwnedDef<MenuBarDefinition> refusedOverlay(MergeMenuBars(0, &overlay));
  loka::app::testing::allowMenuBarDefinitionClones();
  LOKA_VERIFY(!refusedBase.isSet() && !refusedOverlay.isSet());
}
void testMenuMergeCloneRefusalSkipsRowKeepsNextCompletion()
{
  NullPlatformContext platform;
  RecordingApp app;
  const MenuBarDefinition base = defaultBar();
  app.setDefaultMenuBar(&base);
  NullWindow *window = addWindow<1>(app, platform);
  app.setActiveWindow(window);
  // Two failures cover the two admissions; no App-side retry/cache state.
  loka::app::testing::failMenuBarDefinitionClones(2);
  app.operationLoop();
  loka::app::testing::allowMenuBarDefinitionClones();
  LOKA_VERIFY(app.offers.empty());
  app.operationLoop();
  MenuBarDefinition expected = defaultBar();
  expected << (Menu("View") << MenuItem("Zoom"));
  LOKA_VERIFY(app.offers.size() == 2);
  LOKA_VERIFY(app.offers[0].source == window->scene());
  LOKA_VERIFY(app.offers[0].bar.equalsProjection(expected));
}
void testInactiveWindowCloseKeepsActiveBar()
{
  NullPlatformContext platform;
  RecordingApp app;
  const MenuBarDefinition base = defaultBar();
  app.setDefaultMenuBar(&base);
  NullWindow *active = addWindow<1>(app, platform);
  NullWindow *inactive = addWindow<2>(app, platform);
  app.setActiveWindow(active);
  app.operationLoop();
  const Offer before = app.offers[0];
  app.offers.clear();
  Operation turn;
  app.requestWindowClose(inactive);
  LOKA_VERIFY(app.offers.empty());
  turn.settle();
  app.admitAndApplyWindows();
  LOKA_VERIFY(app.offers.size() == 1);
  LOKA_VERIFY(app.offers[0].window == active && app.offers[0].source == before.source);
  LOKA_VERIFY(app.offers[0].bar.equalsProjection(before.bar));
  turn.close();
  app.reclaimWindows();
}
void testDefaultRefreshIsVisibleToTheSameCompletion()
{
  NullPlatformContext platform;
  RefreshConfig config;
  RecordingApp app(&config);
  NullWindow *window = addWindow<0>(app, platform);
  app.setActiveWindow(window);
  app.requestMenuInvalidation();
  LOKA_VERIFY(app.flushMenuInvalidation());
  app.operationLoop();
  LOKA_VERIFY(app.offers.size() == 2);
  // MenuBoundary makes menus opaque; compare snapshots from its own declaration.
  const MenuBarDefinition initial = app.offers[0].bar;
  app.offers.clear();
  {
    Operation turn;
    config.menu.change();
    LOKA_VERIFY(config.menuRefresh().hasPendingRequest());
    // operationLoop joins this turn and performs the modeled rail completion.
    app.operationLoop();
    LOKA_VERIFY(app.offers.size() == 2);
    LOKA_VERIFY(!app.offers[0].bar.equalsProjection(initial));
    LOKA_VERIFY(app.offers[0].bar.menuAt(0)->itemsHead()->title.equals(String::Literal("After")));
    turn.close();
  }
}
void testCleanCompletionsDoNotRecomposeDefault()
{
  NullPlatformContext platform;
  RefreshConfig config;
  RecordingApp app(&config);
  NullWindow *window = addWindow<0>(app, platform);
  app.setActiveWindow(window);
  app.requestMenuInvalidation();
  LOKA_VERIFY(app.flushMenuInvalidation());
  app.operationLoop();
  const int composed = config.composes;
  LOKA_VERIFY(composed >= 1);
  app.offers.clear();
  // Rails clear the controller diff after projecting; a clean completion must
  // still read the cached default instead of recomposing it (codex review).
  for (int i = 0; i < 5; ++i)
  {
    app.clearMenuDiff();
    app.operationLoop();
  }
  LOKA_VERIFY(config.composes == composed);
  LOKA_VERIFY(app.offers.size() == 10 && app.offers[9].hasBar);
}
void testTwoAdmissionsReofferSameSource()
{
  NullPlatformContext platform;
  RecordingApp app;
  NullWindow *window = addWindow<1>(app, platform);
  app.setActiveWindow(window);
  app.operationLoop();
  LOKA_VERIFY(app.offers.size() == 2);
  LOKA_VERIFY(app.offers[0].source == window->scene() && app.offers[1].source == window->scene());
  LOKA_VERIFY(app.offers[0].bar.equalsProjection(app.offers[1].bar));
}
void testBootstrapProjectsOnce()
{
  RecordingApp empty;
  empty.run();
  LOKA_VERIFY(empty.offers.size() == 1 && !empty.offers[0].hasBar);
  LOKA_VERIFY(empty.offers[0].window == 0 && empty.offers[0].source == 0);
  RecordingApp app;
  const MenuBarDefinition base = defaultBar();
  app.setDefaultMenuBar(&base);
  LOKA_VERIFY(app.offers.empty());
  LOKA_VERIFY(!Operation::hasActive());
  app.run();
  LOKA_VERIFY(app.offers.size() == 1);
  LOKA_VERIFY(app.offers[0].window == 0 && app.offers[0].source == 0);
  LOKA_VERIFY(app.offers[0].bar.equalsProjection(base));
  app.offers.clear();
  // No-group completion, distinct from last-close's empty group.
  app.operationLoop();
  LOKA_VERIFY(app.offers.size() == 2 && app.offers[0].bar.equalsProjection(base));
}
