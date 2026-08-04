#include "OwnershipDumpTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../example/HelloWorld/src/MainNode.hpp"
#include "app/PlatformContext.hpp"
#include "app/core/App.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/State.hpp"
#include "support/RecordingPlatformController.hpp"
#include "support/RecomposingBoundary.hpp"
#include "testing/scene/OwnershipDump.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  struct OwnershipDumpPayload
  {
  };

  struct OwnershipDumpScenario
  {
    OwnershipDumpScenario()
        : held(),
          arenaState(),
          dirtySource(0),
          parkedCondition(true),
          creatorOwner(0),
          showCreator(true),
          useConditional(false),
          nestedBoundaryBranch(false),
          createHeld(true),
          showFirstHolder(true),
          showSecondHolder(false),
          createStates(true),
          releaseCount(0)
    {
    }

    loka::core::Held<OwnershipDumpPayload> held;
    loka::app::scene::NodeState<int> arenaState;
    loka::core::MutableState<int> dirtySource;
    loka::core::MutableState<bool> parkedCondition;
    loka::app::scene::IStateOwner *creatorOwner;
    bool showCreator;
    bool useConditional;
    bool nestedBoundaryBranch;
    bool createHeld;
    bool showFirstHolder;
    bool showSecondHolder;
    bool createStates;
    int releaseCount;
  };

  OwnershipDumpScenario *g_ownershipDumpScenario = 0;

  void verifyOwnershipDump(const std::string &actual,
                           const std::string &expected)
  {
    if (actual == expected)
    {
      return;
    }
    std::fprintf(stderr,
                 "ownership dump mismatch\n--- expected ---\n%s--- actual ---\n%s",
                 expected.c_str(),
                 actual.c_str());
    std::abort();
  }

  void releaseOwnershipDumpPayload(OwnershipDumpPayload *payload)
  {
    assert(g_ownershipDumpScenario);
    ++g_ownershipDumpScenario->releaseCount;
    delete payload;
  }

  struct OwnershipDumpProbeTypeTag
  {
  };

  class OwnershipDumpProbeNode;

  struct OwnershipDumpProbeProps
      : public loka::app::scene::NodePropsBase<OwnershipDumpProbeProps>
  {
    typedef OwnershipDumpProbeTypeTag TypeTag;
    typedef OwnershipDumpProbeNode NodeType;

    enum Role
    {
      ROLE_CREATE = 0,
      ROLE_HOLD
    };

    explicit OwnershipDumpProbeProps(Role value = ROLE_HOLD)
        : role(value)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const OwnershipDumpProbeProps &other =
          static_cast<const OwnershipDumpProbeProps &>(rhs);
      return this->role < other.role;
    }

    Role role;
  };

  class OwnershipDumpProbeNode : public loka::app::scene::ComposableNode
  {
  public:
    typedef OwnershipDumpProbeTypeTag TypeTag;
    OwnershipDumpProbeProps props;

    explicit OwnershipDumpProbeNode(const OwnershipDumpProbeProps &value)
        : loka::app::scene::ComposableNode(),
          props(value)
    {
    }

    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<OwnershipDumpProbeNode>();
    }

  protected:
    virtual void composeWithContext(
        loka::app::scene::ComponentContext &context,
        loka::app::scene::ComposeEvent event)
    {
      assert(g_ownershipDumpScenario);
      if (event != loka::app::scene::COMPOSE_EVENT_ATTACH)
      {
        return;
      }
      OwnershipDumpScenario &scenario = *g_ownershipDumpScenario;
      loka::app::scene::NodeComposition composition;
      composition.setContext(&context);
      if (this->props.role == OwnershipDumpProbeProps::ROLE_CREATE)
      {
        scenario.creatorOwner = context.stateOwner();
        if (scenario.createStates && !scenario.arenaState.isValid())
        {
          composition.declareStates().state(scenario.arenaState, 17);
          context.stateOwner()->adoptState(
              new loka::core::MutableState<int>(23));
        }
        if (scenario.createHeld && !scenario.held.isValid())
        {
          scenario.held = composition.hold(
              new OwnershipDumpPayload(),
              &releaseOwnershipDumpPayload);
          if (!scenario.held.isValid())
          {
            std::abort();
          }
        }
        return;
      }
      const loka::core::Held<OwnershipDumpPayload> acquired =
          composition.hold(scenario.held);
      if (!acquired.isValid())
      {
        std::abort();
      }
    }
  };

  typedef loka::app::scene::NodeDefinition<OwnershipDumpProbeProps,
                                           OwnershipDumpProbeNode>
      OwnershipDumpProbeDefinition;

  class OwnershipDumpNestedBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<OwnershipDumpNestedBoundaryNode>
      OwnershipDumpNestedBoundaryProps;

  class OwnershipDumpNestedBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            OwnershipDumpNestedBoundaryNode,
            OwnershipDumpNestedBoundaryProps>
  {
  public:
    explicit OwnershipDumpNestedBoundaryNode(
        const OwnershipDumpNestedBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<
              OwnershipDumpNestedBoundaryNode,
              OwnershipDumpNestedBoundaryProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      loka::app::Section creator(4111);
      creator << OwnershipDumpProbeDefinition(
          OwnershipDumpProbeProps(OwnershipDumpProbeProps::ROLE_CREATE));
      root << creator;
      composition.declare(root);
    }
  };

  class OwnershipDumpRootNode;
  typedef loka::app::scene::BoundaryPropsFor<OwnershipDumpRootNode>
      OwnershipDumpRootProps;

  class OwnershipDumpRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<OwnershipDumpRootNode,
                                                         OwnershipDumpRootProps>
  {
  public:
    explicit OwnershipDumpRootNode(const OwnershipDumpRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<OwnershipDumpRootNode,
                                                    OwnershipDumpRootProps>(props)
    {
      assert(g_ownershipDumpScenario);
    }

    virtual void composeNode(
        loka::app::scene::NodeComposition &composition)
    {
      assert(g_ownershipDumpScenario);
      OwnershipDumpScenario &scenario = *g_ownershipDumpScenario;
      loka::app::Fragment root;
      if (scenario.showCreator)
      {
        loka::app::Section creator(4101);
        creator << OwnershipDumpProbeDefinition(
            OwnershipDumpProbeProps(OwnershipDumpProbeProps::ROLE_CREATE));
        if (scenario.showFirstHolder)
        {
          loka::app::Section holder(4102);
          holder << OwnershipDumpProbeDefinition(
              OwnershipDumpProbeProps(OwnershipDumpProbeProps::ROLE_HOLD));
          creator << holder;
        }
        if (scenario.showSecondHolder)
        {
          loka::app::Section holder(4103);
          holder << OwnershipDumpProbeDefinition(
              OwnershipDumpProbeProps(OwnershipDumpProbeProps::ROLE_HOLD));
          creator << holder;
        }
        if (scenario.useConditional)
        {
          loka::app::FragmentDefinition emptyBranch;
          if (scenario.nestedBoundaryBranch)
          {
            loka::app::FragmentDefinition trueBranch;
            trueBranch << loka::app::scene::Boundary<
                OwnershipDumpNestedBoundaryNode>();
            loka::app::scene::ConditionalDefinition conditional(
                (loka::app::scene::ConditionalProps(
                    &scenario.parkedCondition, &trueBranch, &emptyBranch)));
            root << conditional;
          }
          else
          {
            loka::app::scene::ConditionalDefinition conditional(
                (loka::app::scene::ConditionalProps(
                    &scenario.parkedCondition, &creator, &emptyBranch)));
            root << conditional;
          }
        }
        else
        {
          root << creator;
        }
      }
      composition.declare(root);
    }

    virtual void declareDirtySources(
        loka::app::scene::DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(
          &g_ownershipDumpScenario->dirtySource,
          loka::app::scene::NODE_DIRTY_PROPS);
    }
  };

  class OwnershipDumpTestApp : public App
  {
  public:
    OwnershipDumpTestApp()
        : App(0)
    {
    }

    virtual void quit()
    {
    }

    void install(Window *window)
    {
      this->group_ =
          new AppComponentGroup(std::vector<AppComponent *>(1, window));
      this->setActiveWindow(window);
    }
  };

  class OwnershipDumpPlatformContext : public PlatformContext
  {
  public:
    virtual App *createApp(AppConfigurable *, HINSTANCE, int) const
    {
      return 0;
    }

    virtual Window *createWindow(const WindowProps &)
    {
      return 0;
    }

    virtual loka::app::scene::NodeContext *createNodeContext(
        loka::app::scene::Node *) const
    {
      return 0;
    }

    virtual bool openFile(
        const loka::file::File &,
        loka::platform::file::FileHandle &) const
    {
      return false;
    }

    virtual bool createImageFromBlob(
        const loka::core::resource::Blob &,
        std::size_t,
        std::size_t,
        loka::core::resource::Image &) const
    {
      return false;
    }
  };

  loka::app::scene::Scene *createFixtureScene(
      SceneTestSupport::RecordingPlatformController &platform)
  {
    loka::app::scene::BoundaryDefinition<OwnershipDumpRootProps,
                                         OwnershipDumpRootNode> root;
    loka::app::scene::Scene *scene = new loka::app::scene::Scene(root);
    scene->mount(&platform);
    scene->updateAttached(true);
    loka::app::scene::BoundaryNode *boundary =
        loka::dsl::testing::SceneTestAccess::rootBoundary(*scene);
    if (!boundary)
    {
      std::abort();
    }
    boundary->setTestId("MainView");
    return scene;
  }

  void requestFixtureRecompose(loka::app::scene::Scene &scene)
  {
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
  }
} // namespace

void testOwnershipDumpPinsRepresentativeHelloWorld()
{
  using namespace loka::app::scene;
  NodeDefinition<helloworld::MainProps, helloworld::MainNode> mainDefinition;
  SceneTestSupport::RecordingPlatformController platform;
  Scene scene(mainDefinition.clone());
  scene.mount(&platform);
  scene.updateAttached(true);

  const std::string expected(
      "scene\n"
      "  boundary\n"
      "    boundary\n"
      "      states: 9 (arena 9, heap 0)\n"
      "      observed: 8\n");
  verifyOwnershipDump(
      loka::dsl::testing::OwnershipDump::dump(scene), expected);
}

void testOwnershipDumpPinsFullVocabulary()
{
  OwnershipDumpScenario scenario;
  g_ownershipDumpScenario = &scenario;
  {
    OwnershipDumpPlatformContext context;
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene *scene = createFixtureScene(platform);
    WindowProps props;
    props.scene(scene);
    OwnershipDumpTestApp app;
    app.install(new Window(&context, props));

    const std::string expected(
        "app\n"
        "  window[0]\n"
        "    scene\n"
        "      boundary \"MainView\"\n"
        "        observed: 1\n"
        "        section(4101)\n"
        "          states: 2 (arena 1, heap 1)\n"
        "          held#1 count=2 held-by [section(4101) x1, section(4102) x1]\n"
        "          section(4102)\n"
        "            held#1 count=2 held-by [section(4101) x1, section(4102) x1]\n");
    verifyOwnershipDump(
        loka::dsl::testing::OwnershipDump::dump(app), expected);
  }
  assert(scenario.releaseCount == 1);
  g_ownershipDumpScenario = 0;
}

void testOwnershipDumpHeldByNamesSurvivingOwner()
{
  OwnershipDumpScenario scenario;
  scenario.createStates = false;
  scenario.showSecondHolder = true;
  g_ownershipDumpScenario = &scenario;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene *scene = createFixtureScene(platform);
    if (!scenario.creatorOwner)
    {
      std::abort();
    }
    scenario.creatorOwner->detachHeldResources();
    scenario.showFirstHolder = false;
    requestFixtureRecompose(*scene);

    const std::string expected(
        "scene\n"
        "  boundary \"MainView\"\n"
        "    observed: 1\n"
        "    section(4101)\n"
        "      section(4103)\n"
        "        held#1 count=1 held-by [section(4103) x1]\n");
    verifyOwnershipDump(
        loka::dsl::testing::OwnershipDump::dump(*scene), expected);
    delete scene;
  }
  assert(scenario.releaseCount == 1);
  g_ownershipDumpScenario = 0;
}

void testOwnershipDumpIsDeterministic()
{
  OwnershipDumpScenario scenario;
  g_ownershipDumpScenario = &scenario;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene *scene = createFixtureScene(platform);
    const std::string first =
        loka::dsl::testing::OwnershipDump::dump(*scene);
    const std::string second =
        loka::dsl::testing::OwnershipDump::dump(*scene);
    verifyOwnershipDump(first, second);
    delete scene;
  }
  assert(scenario.releaseCount == 1);
  g_ownershipDumpScenario = 0;
}

void testOwnershipDumpWalksParkedBranches()
{
  OwnershipDumpScenario scenario;
  scenario.useConditional = true;
  scenario.showFirstHolder = false;

  g_ownershipDumpScenario = &scenario;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene *scene = createFixtureScene(platform);

    // Flip the condition: the section-bearing branch parks. Parking is a
    // retained detach, and detach removes slots (axiom 14) -- the branch
    // keeps its states warm for re-entry, but its hold drops, the last drop
    // queues the releaser, and cross-tick retention re-acquires through the
    // owner. A dump that only walked childrenHead() would render the parked
    // states as unowned, which is the exact lie this tool exists to remove.
    // The condition is a registered branch-seat source, so set() itself
    // drives the recompose that parks the branch; the park-time drop queues
    // the releaser on the boundary pool inside that same run.
    scenario.parkedCondition.set(false);
    assert(scenario.releaseCount == 0 &&
           "the drop queues the releaser; nothing may fire at the park site");

    const std::string parkedExpected(
        "scene\n"
        "  boundary \"MainView\"\n"
        "    pending-releases: 1\n"
        "    observed: 2\n"
        "    parked\n"
        "      section(4101)\n"
        "        states: 2 (arena 1, heap 1)\n");
    verifyOwnershipDump(
        loka::dsl::testing::OwnershipDump::dump(*scene), parkedExpected);

    LOKA_VERIFY(!scene->flushInvalidation());
    assert(scenario.releaseCount == 1 &&
           "the queued releaser runs at the drain, on the owner's clock");

    // Re-entry (set(true)) is deliberately not exercised here: flipping back
    // currently loses the seat's branch entirely -- the parked section is
    // taken, reconciliation fails, and the replacement never materializes --
    // independent of the park-time drop. That is a branch-seat defect in the
    // frozen scene-update area, filed separately with this fixture named as
    // the reproducer.
    delete scene;
  }
  assert(scenario.releaseCount == 1 &&
         "the parked payload released exactly once, at the drain");
  g_ownershipDumpScenario = 0;
}

void testOwnershipDumpAdoptsParkedNestedBoundaryReleases()
{
  OwnershipDumpScenario scenario;
  scenario.useConditional = true;
  scenario.nestedBoundaryBranch = true;
  scenario.showFirstHolder = false;
  g_ownershipDumpScenario = &scenario;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene *scene = createFixtureScene(platform);
    assert(scenario.held.isValid());

    // The last drop happens inside the branch being parked, so the releaser
    // first lands on the nested Boundary's pool -- a clock the live-tree
    // drain walk will never reach again. The parking boundary must adopt it.
    scenario.parkedCondition.set(false);
    assert(scenario.releaseCount == 0 &&
           "adoption re-queues; nothing may fire at the park site");

    const std::string parkedExpected(
        "scene\n"
        "  boundary \"MainView\"\n"
        "    pending-releases: 1\n"
        "    observed: 2\n"
        "    parked\n"
        "      boundary\n"
        "        section(4111)\n"
        "          states: 2 (arena 1, heap 1)\n");
    verifyOwnershipDump(
        loka::dsl::testing::OwnershipDump::dump(*scene), parkedExpected);

    LOKA_VERIFY(!scene->flushInvalidation());
    assert(scenario.releaseCount == 1 &&
           "the adopted releaser runs at the live boundary's drain");
    delete scene;
  }
  assert(scenario.releaseCount == 1);
  g_ownershipDumpScenario = 0;
}

void testOwnershipDumpShowsPendingReleaseUntilDrain()
{
  OwnershipDumpScenario scenario;
  scenario.createStates = false;
  scenario.showFirstHolder = false;
  g_ownershipDumpScenario = &scenario;
  {
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene *scene = createFixtureScene(platform);
    scenario.showCreator = false;
    requestFixtureRecompose(*scene);

    const std::string pendingExpected(
        "scene\n"
        "  boundary \"MainView\"\n"
        "    pending-releases: 1\n"
        "    observed: 1\n");
    verifyOwnershipDump(
        loka::dsl::testing::OwnershipDump::dump(*scene), pendingExpected);
    LOKA_VERIFY(!scene->flushInvalidation());
    const std::string drainedExpected(
        "scene\n"
        "  boundary \"MainView\"\n"
        "    observed: 1\n");
    verifyOwnershipDump(
        loka::dsl::testing::OwnershipDump::dump(*scene), drainedExpected);
    delete scene;
  }
  assert(scenario.releaseCount == 1);
  g_ownershipDumpScenario = 0;
}
