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
  class BootstrapConfig : public AppConfigurable
  {
  public:
    explicit BootstrapConfig(PlatformContext *platform) : AppConfigurable(platform) {}
    virtual void compose(AppComposition &) {}
    virtual void composeDefaultMenu(MenuComposition &c)
    {
      this->compositions.push_back(defaultBar());
      c << this->compositions.back();
    }
    std::vector<MenuBarDefinition> compositions;
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
  LOKA_VERIFY(app.offers[0].window == window && app.offers[0].bar.equalsStructure(bar));
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
  LOKA_VERIFY(app.offers[0].bar.equalsStructure(bar));
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
  LOKA_VERIFY(app.offers[0].bar.equalsStructure(expectedA));
  LOKA_VERIFY(app.offers[1].window == b && app.offers[1].source == b->scene());
  LOKA_VERIFY(app.offers[1].bar.equalsStructure(expectedB));
  LOKA_VERIFY(app.offers[2].window == plain && app.offers[2].source == 0);
  LOKA_VERIFY(app.offers[2].bar.equalsStructure(base));
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
  LOKA_VERIFY(merged.isSet() && merged->equalsStructure(expected));
  LOKA_VERIFY(base.equalsStructure(defaultBar()));
  OwnedDef<MenuBarDefinition> baseOnly(MergeMenuBars(&base, 0));
  OwnedDef<MenuBarDefinition> overlayOnly(MergeMenuBars(0, &overlay));
  LOKA_VERIFY(baseOnly.isSet() && baseOnly->equalsStructure(base));
  LOKA_VERIFY(overlayOnly.isSet() && overlayOnly->equalsStructure(overlay));
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
  LOKA_VERIFY(app.offers[0].bar.equalsStructure(expected));
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
  LOKA_VERIFY(app.offers[0].bar.equalsStructure(before.bar));
  turn.close();
  app.reclaimWindows();
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
  LOKA_VERIFY(app.offers[0].bar.equalsStructure(app.offers[1].bar));
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
  LOKA_VERIFY(app.offers[0].bar.equalsStructure(base));
  app.offers.clear();
  // No-group completion, distinct from last-close's empty group.
  app.operationLoop();
  LOKA_VERIFY(app.offers.size() == 2 && app.offers[0].bar.equalsStructure(base));
}

void testDefaultBarComposedOnceAtBootstrap()
{
  NullPlatformContext platform;
  BootstrapConfig config(&platform);
  RecordingApp app(&config);
  app.run();
  for (int i = 0; i < 5; ++i)
    app.operationLoop();
  LOKA_VERIFY(config.compositions.size() == 1);
  LOKA_VERIFY(app.defaultMenuBar() != 0);
  LOKA_VERIFY(app.offers.size() == 11);
  for (size_t i = 0; i < app.offers.size(); ++i)
    LOKA_VERIFY(app.offers[i].hasBar && app.offers[i].bar.equalsStructure(defaultBar()));
}

void testDefaultBarBootstrapCloneRefusalLeavesNoDefault()
{
  NullPlatformContext platform;
  BootstrapConfig config(&platform);
  RecordingApp app(&config);
  loka::app::testing::failNextMenuBarDefinitionClone();
  app.run();
  loka::app::testing::allowMenuBarDefinitionClones();
  LOKA_VERIFY(app.defaultMenuBar() == 0);
  NullWindow *window = addWindow<1>(app, platform);
  app.setActiveWindow(window);
  app.offers.clear();
  for (int i = 0; i < 5; ++i)
    app.operationLoop();
  LOKA_VERIFY(config.compositions.size() == 1);
  LOKA_VERIFY(app.defaultMenuBar() == 0);
  MenuBarDefinition expected;
  expected << (Menu("View") << MenuItem("Zoom"));
  LOKA_VERIFY(app.offers.size() == 10);
  for (size_t i = 0; i < app.offers.size(); ++i)
  {
    LOKA_VERIFY(app.offers[i].window == window && app.offers[i].source == window->scene());
    LOKA_VERIFY(app.offers[i].hasBar && app.offers[i].bar.equalsStructure(expected));
  }
}

void testDefaultBarReplacementCloneRefusalPreservesInstalledValue()
{
  RecordingApp app;
  const MenuBarDefinition initial = defaultBar();
  MenuBarDefinition replacement;
  replacement << (Menu("Other") << MenuItem("Replace"));
  app.setDefaultMenuBar(&initial);
  const MenuBarDefinition *installed = app.defaultMenuBar();
  LOKA_VERIFY(installed != 0);
  loka::app::testing::failNextMenuBarDefinitionClone();
  app.setDefaultMenuBar(&replacement);
  loka::app::testing::allowMenuBarDefinitionClones();
  LOKA_VERIFY(app.defaultMenuBar() == installed);
  app.operationLoop();
  LOKA_VERIFY(app.offers.size() == 2);
  for (size_t i = 0; i < app.offers.size(); ++i)
    LOKA_VERIFY(app.offers[i].hasBar && app.offers[i].bar.equalsStructure(initial));
  app.setDefaultMenuBar(0);
  LOKA_VERIFY(app.defaultMenuBar() == 0);
}
