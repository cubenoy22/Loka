#include "DefinitionCloneTests.hpp"
#include <cassert>
#include <cstdio>
#include "core/State.hpp"
#include "core/util/OwnedDef.hpp"
#include "app/PlatformContext.hpp"
#include "app/Menu.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/MenuController.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/composition/NodeCompositionSnapshot.hpp"
#include "app/scene/node/Conditional.hpp"

// Regression pin for the ConditionalDefinition dangling-branch fix (issue #47,
// docs/TODO.md "DSL definition lifetime safety"): ConditionalDefinition must own
// deep copies of its branch definitions so that stack-local/temporary branch
// definitions can be destroyed without leaving the conditional (or its clones)
// pointing at freed memory. Pre-fix code fails this under ASan.

// ============================================================
// Probe definition/node with liveness counters
// ============================================================

namespace
{

  class CloneProbeNode;
  struct CloneProbeTypeTag
  {
  };

  static int g_probePropsAlive = 0;
  static int g_probeNodesAlive = 0;
  static int g_probeNodesCreated = 0;
  static int g_limitedCloneBudget = -1;
  static int g_limitedCloneCalls = 0;

  struct CloneProbeProps : public loka::app::scene::NodePropsBase<CloneProbeProps>
  {
    typedef CloneProbeTypeTag TypeTag;
    typedef CloneProbeNode NodeType;
    CloneProbeProps()
    {
      ++g_probePropsAlive;
    }
    CloneProbeProps(const CloneProbeProps &other)
        : loka::app::scene::NodePropsBase<CloneProbeProps>(other)
    {
      ++g_probePropsAlive;
    }
    ~CloneProbeProps()
    {
      --g_probePropsAlive;
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
  };

  class CloneProbeNode : public loka::app::scene::Node
  {
  public:
    typedef CloneProbeTypeTag TypeTag;
    CloneProbeProps props;
    CloneProbeNode(const CloneProbeProps &p)
        : props(p)
    {
      ++g_probeNodesAlive;
      ++g_probeNodesCreated;
    }
    virtual ~CloneProbeNode()
    {
      --g_probeNodesAlive;
    }
  };

  struct CloneProbeDefinition : public loka::app::scene::NodeDefinition<CloneProbeProps, CloneProbeNode>
  {
    CloneProbeDefinition()
        : loka::app::scene::NodeDefinition<CloneProbeProps, CloneProbeNode>()
    {
    }
  };

  struct OomCloneProbeDefinition : public CloneProbeDefinition
  {
    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return 0;
    }
  };

  struct LimitedCloneProbeDefinition : public CloneProbeDefinition
  {
    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      ++g_limitedCloneCalls;
      if (g_limitedCloneBudget == 0)
      {
        return 0;
      }
      if (g_limitedCloneBudget > 0)
      {
        --g_limitedCloneBudget;
      }
      return new LimitedCloneProbeDefinition(*this);
    }
  };

  struct NullWindowPlatformContext : public PlatformContext
  {
    NullWindowPlatformContext()
        : createWindowCalls(0),
          sawInitialScene(false),
          sawRootDefinition(false)
    {
    }

    virtual App *createApp(AppConfigurable *, HINSTANCE, int) const
    {
      return 0;
    }

    virtual Window *createWindow(const WindowProps &props)
    {
      ++createWindowCalls;
      sawInitialScene = props.peekInitialScene() != 0;
      sawRootDefinition = props.rootDefinition != 0;
      return 0;
    }

    virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *) const
    {
      return 0;
    }

    virtual bool openFile(const loka::file::File &, loka::platform::file::FileHandle &) const
    {
      return false;
    }

    virtual bool createImageFromBlob(const loka::core::resource::Blob &, loka::core::resource::Image &) const
    {
      return false;
    }

    int createWindowCalls;
    bool sawInitialScene;
    bool sawRootDefinition;
  };

  class MenuCloneTestConfig : public AppConfigurable
  {
  public:
    MenuCloneTestConfig()
        : AppConfigurable(0),
          includeSecondMenu(false)
    {
    }

    virtual void compose(AppComposition &)
    {
    }

    virtual void composeMenu(loka::app::MenuComposition &composition)
    {
      composition << loka::app::Menu("Replacement");
      if (includeSecondMenu)
      {
        composition << loka::app::Menu("Second");
      }
    }

    bool includeSecondMenu;
  };

  void CountMenuApply(void *userData, Window *)
  {
    int *applyCount = static_cast<int *>(userData);
    ++*applyCount;
  }

  bool hasSingleMenuNamed(const loka::app::MenuBarDefinition *menuBar, const char *title)
  {
    return menuBar && menuBar->menusCount() == 1 && menuBar->menuAt(0)
           && menuBar->menuAt(0)->title.equals(loka::core::String::Literal(title));
  }
} // namespace

// ============================================================
// Tests
// ============================================================

void testOwnedDefOwnership()
{
  printf("\n==== [testOwnedDefOwnership] start ====\n");

  int baseline = g_probePropsAlive;
  {
    loka::core::OwnedDef<CloneProbeDefinition> owned(new CloneProbeDefinition());
    assert(owned.isSet());
    assert(owned.get() != 0);
    assert(g_probePropsAlive == baseline + 1);

    CloneProbeDefinition *taken = owned.take();
    assert(!owned.isSet());
    assert(owned.get() == 0);
    delete taken;
    assert(g_probePropsAlive == baseline);

    owned.reset(new CloneProbeDefinition());
    CloneProbeDefinition *same = owned.get();
    owned.reset(same);
    assert(g_probePropsAlive == baseline + 1);
    owned.reset();
    assert(!owned.isSet());
    assert(g_probePropsAlive == baseline);
  }
  {
    loka::core::OwnedDef<CloneProbeDefinition> owned(new CloneProbeDefinition());
    assert(g_probePropsAlive == baseline + 1);
  }
  assert(g_probePropsAlive == baseline);
  {
    OomCloneProbeDefinition failedClone;
    loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> empty(failedClone.clone());
    assert(!empty.isSet());
    assert(empty.get() == 0);
  }
  assert(g_probePropsAlive == baseline);

  printf("==== [testOwnedDefOwnership] end ====\n");
}

void testConditionalDefinitionCloneOwnership()
{
  printf("\n==== [testConditionalDefinitionCloneOwnership] start ====\n");

  using loka::app::scene::ConditionalDefinition;
  using loka::app::scene::ConditionalProps;

  // --- the definition owns deep copies of its branch definitions ---
  // The branch definition is stack-local and destroyed before the conditional
  // is cloned or materialized; the owned clones must keep working.
  {
    loka::core::MutableState<bool> cond(true);
    ConditionalDefinition *conditional = 0;
    {
      CloneProbeDefinition trueBranch;
      conditional = new ConditionalDefinition(ConditionalProps(&cond, &trueBranch, 0));
    }
    // trueBranch is gone. Clone the conditional, then destroy the original:
    // the clone must own its own independent branch copy.
    loka::app::scene::NodeDefinitionBase *copy = conditional->clone();
    delete conditional;

    int createdBefore = g_probeNodesCreated;
    loka::app::scene::Node *node = copy->create();
    assert(node != 0);
    assert(g_probeNodesCreated > createdBefore); // true branch materialized from the owned clone
    delete node;
    delete copy;
  }

  // --- assignment deep-copies; self-assignment is safe ---
  {
    loka::core::MutableState<bool> cond(false);
    {
      CloneProbeDefinition falseBranch;
      ConditionalDefinition first(ConditionalProps(&cond, 0, &falseBranch));
      ConditionalDefinition second(ConditionalProps(&cond, 0, 0));
      second = first;
      second = second; // self-assignment must not free the owned branches

      loka::app::scene::Node *node = second.create();
      assert(node != 0);
      delete node;
    }
  }

  // --- ownership balance: every owned copy destroyed exactly once ---
  // A double delete or a leaked branch clone shows up here (and under ASan).
  assert(g_probePropsAlive == 0);
  assert(g_probeNodesAlive == 0);

  printf("==== [testConditionalDefinitionCloneOwnership] end ====\n");
}

void testNestableDefinitionAssignmentPreservesStableChildOnOomClone()
{
  printf("\n==== [testNestableDefinitionAssignmentPreservesStableChildOnOomClone] start ====\n");

  using loka::app::Box;
  using loka::app::BoxDefinition;

  CloneProbeDefinition originalChild;
  BoxDefinition stableTarget = Box().padding(10) << originalChild;
  BoxDefinition sourceWithBadChild;
  sourceWithBadChild.padding(20);
  sourceWithBadChild << new OomCloneProbeDefinition();

  stableTarget = sourceWithBadChild;

  assert(stableTarget.props.padding == 10);
  assert(stableTarget.childrenCount() == 1);
  assert(stableTarget.childrenHead() != 0);
  loka::app::scene::NodeDefinitionBase *childClone = stableTarget.childrenHead()->clone();
  assert(childClone != 0);
  delete childClone;

  printf("==== [testNestableDefinitionAssignmentPreservesStableChildOnOomClone] end ====\n");
}

void testNestableDefinitionCloneReturnsNullOnOomChildClone()
{
  printf("\n==== [testNestableDefinitionCloneReturnsNullOnOomChildClone] start ====\n");

  using loka::app::BoxDefinition;

  BoxDefinition sourceWithBadChild;
  sourceWithBadChild.padding(20);
  sourceWithBadChild << new OomCloneProbeDefinition();

  loka::app::scene::NodeDefinitionBase *copy = sourceWithBadChild.clone();
  assert(copy == 0);

  printf("==== [testNestableDefinitionCloneReturnsNullOnOomChildClone] end ====\n");
}

void testCompositionSnapshotClearsStaleRootOnOomClone()
{
  printf("\n==== [testCompositionSnapshotClearsStaleRootOnOomClone] start ====\n");

  loka::app::scene::NodeCompositionSnapshot snapshot;
  {
    loka::app::scene::NodeComposition stableComposition;
    CloneProbeDefinition stableRoot;
    stableComposition.declare(stableRoot);
    snapshot.capture(stableComposition);
  }
  assert(!snapshot.empty());

  {
    loka::app::scene::NodeComposition failingComposition;
    LimitedCloneProbeDefinition failingRoot;
    g_limitedCloneBudget = 1;
    g_limitedCloneCalls = 0;
    failingComposition.declare(failingRoot);
    snapshot.capture(failingComposition);
    g_limitedCloneBudget = -1;
  }

  assert(g_limitedCloneCalls == 2);
  assert(snapshot.empty());

  printf("==== [testCompositionSnapshotClearsStaleRootOnOomClone] end ====\n");
}

void testNodeCompositionSkipsOomClones()
{
  printf("\n==== [testNodeCompositionSkipsOomClones] start ====\n");

  {
    loka::app::scene::NodeComposition composition;
    CloneProbeDefinition stableRoot;
    OomCloneProbeDefinition failingDefinition;
    loka::app::scene::NodeDefinitionBase &failingBase = failingDefinition;

    composition.declare(stableRoot);
    loka::app::scene::NodeDefinitionBase *storedRoot = composition.root();
    assert(storedRoot != 0);

    loka::app::scene::NodeDefinitionBase *grouped = composition.group(failingDefinition);
    OomCloneProbeDefinition &typedResult = composition.declare(failingDefinition);
    loka::app::scene::NodeDefinitionBase &baseResult = composition.declare(failingBase);
    loka::app::scene::NodeDefinitionBase &taggedResult = composition.declareTagged(42, failingBase);
    OomCloneProbeDefinition &typedTaggedResult = composition.declareTagged(42, failingDefinition);

    assert(grouped == 0);
    assert(&typedResult == &failingDefinition);
    assert(&baseResult == &failingBase);
    assert(&taggedResult == &failingBase);
    assert(&typedTaggedResult == &failingDefinition);
    assert(composition.root() == storedRoot);
  }
  assert(g_probePropsAlive == 0);
  assert(g_probeNodesAlive == 0);

  printf("==== [testNodeCompositionSkipsOomClones] end ====\n");
}

void testWindowPropsAssignmentPreservesOwnedSceneOnOomClone()
{
  printf("\n==== [testWindowPropsAssignmentPreservesOwnedSceneOnOomClone] start ====\n");

  WindowProps stableTarget;
  stableTarget.rootDefinition = new CloneProbeDefinition();

  WindowProps sourceWithBadRoot;
  sourceWithBadRoot.rootDefinition = new OomCloneProbeDefinition();

  stableTarget = sourceWithBadRoot;

  assert(stableTarget.rootDefinition != 0);
  loka::app::scene::NodeDefinitionBase *childClone = stableTarget.rootDefinition->clone();
  assert(childClone != 0);
  delete childClone;

  printf("==== [testWindowPropsAssignmentPreservesOwnedSceneOnOomClone] end ====\n");
}

void testWindowDefinitionCreateReturnsNullOnOomRootClone()
{
  printf("\n==== [testWindowDefinitionCreateReturnsNullOnOomRootClone] start ====\n");

  WindowDefinition<WindowProps> def;
  def.props.rootDefinition = new OomCloneProbeDefinition();

  NullWindowPlatformContext context;
  Window *window = def.create(&context);

  assert(window == 0);
  assert(context.createWindowCalls == 0);

  printf("==== [testWindowDefinitionCreateReturnsNullOnOomRootClone] end ====\n");
}

void testWindowDefinitionCreateTransfersSingleRootClone()
{
  printf("\n==== [testWindowDefinitionCreateTransfersSingleRootClone] start ====\n");

  WindowDefinition<WindowProps> def;
  def.props.rootDefinition = new LimitedCloneProbeDefinition();

  NullWindowPlatformContext context;
  g_limitedCloneBudget = 1;
  g_limitedCloneCalls = 0;
  Window *window = def.create(&context);
  g_limitedCloneBudget = -1;

  assert(window == 0);
  assert(g_limitedCloneCalls == 1);
  assert(context.createWindowCalls == 1);
  assert(context.sawInitialScene);
  assert(!context.sawRootDefinition);

  printf("==== [testWindowDefinitionCreateTransfersSingleRootClone] end ====\n");
}

void testMenuControllerPreservesDefaultMenuBarOnOomClone()
{
  printf("\n==== [testMenuControllerPreservesDefaultMenuBarOnOomClone] start ====\n");

  MenuCloneTestConfig config;
  int applyCount = 0;
  MenuController controller(&config, &CountMenuApply, &applyCount);
  loka::app::MenuBarDefinition stableMenuBar;
  stableMenuBar << loka::app::Menu("Stable");
  controller.setDefaultMenuBar(&stableMenuBar, 0);
  assert(hasSingleMenuNamed(controller.defaultMenuBar(), "Stable"));
  assert(applyCount == 1);

  loka::app::MenuBarDefinition replacementMenuBar;
  replacementMenuBar << loka::app::Menu("Replacement");
  loka::app::testing::failNextMenuBarDefinitionClone();
  controller.setDefaultMenuBar(&replacementMenuBar, 0);

  assert(hasSingleMenuNamed(controller.defaultMenuBar(), "Stable"));
  assert(applyCount == 1);
  loka::app::testing::allowMenuBarDefinitionClones();

  printf("==== [testMenuControllerPreservesDefaultMenuBarOnOomClone] end ====\n");
}

void testMenuControllerPreservesRefreshedMenuBarOnOomClone()
{
  printf("\n==== [testMenuControllerPreservesRefreshedMenuBarOnOomClone] start ====\n");

  MenuCloneTestConfig config;
  int applyCount = 0;
  MenuController controller(&config, &CountMenuApply, &applyCount);
  loka::app::MenuBarDefinition stableMenuBar;
  stableMenuBar << loka::app::Menu("Stable");
  controller.setDefaultMenuBar(&stableMenuBar, 0);
  controller.clearDiff();
  config.includeSecondMenu = true;

  loka::app::testing::failNextMenuBarDefinitionClone();
  controller.requestInvalidation();
  bool refreshed = controller.flushInvalidation(0);

  assert(!refreshed);
  assert(hasSingleMenuNamed(controller.defaultMenuBar(), "Stable"));
  assert(!controller.diff().valid);
  assert(applyCount == 1);
  loka::app::testing::allowMenuBarDefinitionClones();

  printf("==== [testMenuControllerPreservesRefreshedMenuBarOnOomClone] end ====\n");
}
