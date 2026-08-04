#include "ComponentNodeTests.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#include <cstddef>

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/nestable/BoundarySection.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "app/scene/node/Conditional.hpp"
#include "core/LokaAlloc.hpp"
#include "support/RecomposingBoundary.hpp"
#include "support/RecordingPlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{

  /** Copy-counting resident so retirement pins can watch the component's
      owned storage live and die (same idiom as SectionTrackedValue). */
  struct ComponentTrackedValue
  {
    ComponentTrackedValue(int *aliveCount, int v)
        : alive(aliveCount),
          value(v)
    {
      if (this->alive)
      {
        ++*this->alive;
      }
    }

    ComponentTrackedValue(const ComponentTrackedValue &other)
        : alive(other.alive),
          value(other.value)
    {
      if (this->alive)
      {
        ++*this->alive;
      }
    }

    ~ComponentTrackedValue()
    {
      if (this->alive)
      {
        --*this->alive;
      }
    }

    bool operator!=(const ComponentTrackedValue &other) const
    {
      return this->alive != other.alive || this->value != other.value;
    }

    int *alive;
    int value;
  };

  struct ComponentProbeObservation
  {
    ComponentProbeObservation()
        : composeChildrenCalls(0),
          statesValidAtComposeChildren(0),
          attachCalls(0),
          lastOwner(0),
          lastArenaAllocated(false)
    {
    }

    int composeChildrenCalls;
    int statesValidAtComposeChildren;
    int attachCalls;
    loka::app::scene::IStateOwner *lastOwner;
    bool lastArenaAllocated;
  };

  struct TestCellComponentTypeTag
  {
  };

  class TestCellComponentNode;

  struct TestCellComponentProps
      : public loka::app::scene::NodePropsBase<TestCellComponentProps>
  {
    typedef TestCellComponentTypeTag TypeTag;
    typedef TestCellComponentNode NodeType;

    TestCellComponentProps()
        : observation(0),
          trackedAlive(0),
          revision(0)
    {
    }

    TestCellComponentProps(ComponentProbeObservation *value,
                           int *trackedAliveCount,
                           int revisionValue)
        : observation(value),
          trackedAlive(trackedAliveCount),
          revision(revisionValue)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return this->propsTypeId() < rhs.propsTypeId();
      }
      const TestCellComponentProps &other =
          static_cast<const TestCellComponentProps &>(rhs);
      if (this->observation != other.observation)
      {
        return this->observation < other.observation;
      }
      if (this->trackedAlive != other.trackedAlive)
      {
        return this->trackedAlive < other.trackedAlive;
      }
      return this->revision < other.revision;
    }

    ComponentProbeObservation *observation;
    int *trackedAlive;
    int revision;
  };

  /** The primitive under test: residents declared in the constructor, one
      Cell control child wired to a resident's pointer in composeChildren. */
  class TestCellComponentNode : public loka::app::scene::ComponentNode
  {
  public:
    typedef TestCellComponentTypeTag TypeTag;
    TestCellComponentProps props;

    explicit TestCellComponentNode(const TestCellComponentProps &p)
        : loka::app::scene::ComponentNode(),
          props(p),
          text_(),
          tracked_()
    {
      this->state(this->text_, loka::core::String::Literal("."));
      this->state(this->tracked_,
                  ComponentTrackedValue(p.trackedAlive, 41));
    }

    loka::app::scene::NodeState<loka::core::String> &text()
    {
      return this->text_;
    }

  protected:
    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      if (this->props.observation)
      {
        ++this->props.observation->attachCalls;
      }
    }

    virtual void composeChildren(loka::app::scene::NodeComposition &c)
    {
      if (this->props.observation)
      {
        ComponentProbeObservation &obs = *this->props.observation;
        ++obs.composeChildrenCalls;
        if (this->text_.isValid() && this->tracked_.isValid())
        {
          ++obs.statesValidAtComposeChildren;
        }
        obs.lastOwner = this->text_.isValid() ? this->text_.dangerouslyOwner() : 0;
        obs.lastArenaAllocated =
            this->text_.isValid() &&
            this->text_.dangerouslyMutableState()->isArenaAllocated();
      }
      loka::app::Cell cell(this->text_.state());
      c.declare(cell);
    }

  private:
    loka::app::scene::NodeState<loka::core::String> text_;
    loka::app::scene::NodeState<ComponentTrackedValue> tracked_;
  };

  typedef loka::app::scene::NodeDefinition<TestCellComponentProps,
                                           TestCellComponentNode>
      TestCellComponentDefinition;

  ComponentProbeObservation *g_componentObservation = 0;
  int *g_componentTrackedAlive = 0;
  bool g_componentHostUseSection = true;
  loka::core::MutableState<bool> *g_componentHostCondition = 0;

  class ComponentHostRootNode;
  typedef loka::app::scene::BoundaryPropsFor<ComponentHostRootNode>
      ComponentHostRootProps;

  class ComponentHostRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<ComponentHostRootNode,
                                                         ComponentHostRootProps>
  {
  public:
    explicit ComponentHostRootNode(const ComponentHostRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<ComponentHostRootNode,
                                                    ComponentHostRootProps>(props),
          useSection_(g_componentHostUseSection),
          key_(6001),
          revision_(0)
    {
    }

    // Keep condition flips scheduled instead of flushing mid-set, so the
    // park/reenter pin can observe each phase deterministically.
    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      TestCellComponentDefinition component(
          TestCellComponentProps(g_componentObservation,
                                 g_componentTrackedAlive,
                                 this->revision_));
      if (this->useSection_)
      {
        loka::app::Section section(this->key_);
        section << component;
        root << section;
      }
      else
      {
        root << component;
      }
      composition.declare(root);
    }

    void setKey(loka::app::scene::NodeTag key)
    {
      this->key_ = key;
    }

    void setRevision(int revision)
    {
      this->revision_ = revision;
    }

    static loka::app::scene::Node *findByTag(loka::app::scene::Node *node,
                                             loka::app::scene::NodeTag key)
    {
      if (!node)
      {
        return 0;
      }
      if (node->nodeTag() == key)
      {
        return node;
      }
      loka::app::scene::INestable *nestable = node->asNestable();
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(
          nestable ? nestable->childrenHead() : 0,
          nestable ? nestable->childrenCount() : 0);
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        loka::app::scene::Node *found = findByTag(child, key);
        if (found)
        {
          return found;
        }
      }
      return 0;
    }

    loka::app::BoundarySectionNode *section(loka::app::scene::NodeTag key) const
    {
      loka::app::scene::Node *found =
          findByTag(this->compositionRootNode(), key);
      return found ? found->asBoundarySectionNode() : 0;
    }

    TestCellComponentNode *component(loka::app::scene::NodeTag key) const
    {
      if (!this->useSection_)
      {
        loka::app::scene::Node *root = this->compositionRootNode();
        loka::app::scene::INestable *nestable = root ? root->asNestable() : 0;
        return static_cast<TestCellComponentNode *>(
            nestable ? nestable->childrenHead() : 0);
      }
      loka::app::BoundarySectionNode *host = this->section(key);
      return static_cast<TestCellComponentNode *>(
          host ? host->childrenHead() : 0);
    }

  private:
    bool useSection_;
    loka::app::scene::NodeTag key_;
    int revision_;
  };

  class ComponentParkHostRootNode;
  typedef loka::app::scene::BoundaryPropsFor<ComponentParkHostRootNode>
      ComponentParkHostRootProps;

  /** Non-recomposing host for the park/reenter pin: seat flips must travel
      the branch-seat apply path alone, so reentry re-attaches the retained
      subtree instead of rebuilding a fresh one (H7 pins the recomposing
      variant). */
  class ComponentParkHostRootNode
      : public loka::app::scene::BoundaryNodeFor<ComponentParkHostRootNode>
  {
  public:
    explicit ComponentParkHostRootNode(const ComponentParkHostRootProps &props)
        : loka::app::scene::BoundaryNodeFor<ComponentParkHostRootNode>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      TestCellComponentDefinition component(
          TestCellComponentProps(g_componentObservation,
                                 g_componentTrackedAlive,
                                 0));
      loka::app::Section section(6001);
      section << component;
      loka::app::scene::ConditionalDefinition conditional(
          loka::app::scene::ConditionalProps(
              static_cast<loka::core::State<bool> *>(g_componentHostCondition),
              &section,
              0));
      conditional.setNodeTag(6901);
      root << conditional;
      composition.declare(root);
    }

    TestCellComponentNode *component(loka::app::scene::NodeTag key) const
    {
      loka::app::scene::Node *found =
          ComponentHostRootNode::findByTag(this->compositionRootNode(), key);
      loka::app::BoundarySectionNode *host =
          found ? found->asBoundarySectionNode() : 0;
      return static_cast<TestCellComponentNode *>(
          host ? host->childrenHead() : 0);
    }
  };

  struct ComponentScenario
  {
    ComponentScenario()
        : observation(),
          trackedAlive(0)
    {
      g_componentObservation = &this->observation;
      g_componentTrackedAlive = &this->trackedAlive;
    }

    ~ComponentScenario()
    {
      g_componentObservation = 0;
      g_componentTrackedAlive = 0;
    }

    ComponentProbeObservation observation;
    int trackedAlive;
  };

} // namespace

void testComponentComposesChildrenOnceAfterStatesConnect()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  {
    ComponentScenario scenario;
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<ComponentHostRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    ComponentHostRootNode *root = static_cast<ComponentHostRootNode *>(
        loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    assert(root);
    TestCellComponentNode *component = root->component(6001);
    assert(component);

    // The single door ran exactly once, strictly after the residents
    // connected: composeChildren observed both states valid.
    assert(scenario.observation.composeChildrenCalls == 1);
    assert(scenario.observation.statesValidAtComposeChildren == 1);

    // The child subtree exists and is the declared Cell control, wired to
    // the component-owned resident.
    loka::app::scene::INestable *nestable = component->asNestable();
    assert(nestable && nestable->childrenCount() == 1);
    loka::app::scene::Node *child = nestable->childrenHead();
    (void)child;
    assert(child && child->kind() == loka::app::scene::NODE_KIND_CELL);

    // A recompose over the same identity leaves the structure alone.
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
    assert(root->component(6001) == component);
    assert(component->asNestable()->childrenHead() == child);
    assert(scenario.observation.composeChildrenCalls == 1);
  }
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint(
      "testComponentComposesChildrenOnceAfterStatesConnect");
#endif
}

void testComponentStatesResolveNearestSectionOwner()
{
  ComponentScenario scenario;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<ComponentHostRootNode>()));
  scene.mount(&platform);
  scene.updateAttached(true);

  ComponentHostRootNode *root = static_cast<ComponentHostRootNode *>(
      loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  assert(root);
  loka::app::BoundarySectionNode *section = root->section(6001);
  (void)section;
  assert(section);

  // The component's residents live in the enclosing Section's owner scope,
  // on the boundary arena rather than the heap.
  assert(scenario.observation.lastOwner ==
         static_cast<loka::app::scene::IStateOwner *>(section));
  assert(scenario.observation.lastArenaAllocated);
}

void testComponentStatesFallBackToBoundaryOwnerWithoutSection()
{
  ComponentScenario scenario;
  g_componentHostUseSection = false;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<ComponentHostRootNode>()));
  scene.mount(&platform);
  scene.updateAttached(true);
  g_componentHostUseSection = true;

  ComponentHostRootNode *root = static_cast<ComponentHostRootNode *>(
      loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  assert(root);
  TestCellComponentNode *component = root->component(0);
  (void)component;
  assert(component);
  assert(scenario.observation.lastOwner ==
         static_cast<loka::app::scene::IStateOwner *>(root));
  assert(scenario.observation.composeChildrenCalls == 1);
  assert(scenario.observation.statesValidAtComposeChildren == 1);
}

void testComponentPropsReapplyWithoutTouchingSubtree()
{
  ComponentScenario scenario;
  SceneTestSupport::RecordingPlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<ComponentHostRootNode>()));
  scene.mount(&platform);
  scene.updateAttached(true);

  ComponentHostRootNode *root = static_cast<ComponentHostRootNode *>(
      loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  assert(root);
  TestCellComponentNode *component = root->component(6001);
  assert(component);
  assert(component->props.revision == 0);
  loka::app::scene::Node *child = component->asNestable()->childrenHead();
  (void)child;
  assert(child);

  // A props change over the same identity re-applies props (the plan stops
  // at the composable: reconcileParkedBranch returns at asComposable) and
  // must not rebuild or retire the component's subtree.
  root->setRevision(7);
  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  LOKA_VERIFY(scene.flushInvalidation());

  assert(root->component(6001) == component);
  assert(component->props.revision == 7);
  assert(component->asNestable()->childrenHead() == child);
  assert(scenario.observation.composeChildrenCalls == 1);
}

void testComponentParkedReentryKeepsSubtreeSingular()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  {
    ComponentScenario scenario;
    loka::core::MutableState<bool> condition(true);
    g_componentHostCondition = &condition;
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<ComponentParkHostRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    ComponentParkHostRootNode *root = static_cast<ComponentParkHostRootNode *>(
        loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    assert(root);
    TestCellComponentNode *component = root->component(6001);
    assert(component);
    assert(scenario.observation.composeChildrenCalls == 1);
    loka::app::scene::Node *child = component->asNestable()->childrenHead();
    (void)child;
    assert(child);
    const int aliveWhileShown = scenario.trackedAlive;
    (void)aliveWhileShown;

    // Park: flipping to the null branch detaches the subtree retained.
    condition.set(false);
    assert(scene.hasPendingInvalidation());
    LOKA_VERIFY(scene.flushInvalidation());
    assert(component->lifecycleFact() ==
           loka::app::scene::NODE_FACT_DETACHED_RETAINED);
    assert(scenario.trackedAlive == aliveWhileShown &&
           "a parked component keeps its residents");

    // Reenter: the re-attach must not materialize a second subtree next to
    // the retained one — the childrenHead guard, not a flag, carries this.
    condition.set(true);
    LOKA_VERIFY(scene.flushInvalidation());
    assert(root->component(6001) == component);
    assert(component->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED);
    assert(component->asNestable()->childrenCount() == 1);
    assert(component->asNestable()->childrenHead() == child);
    assert(scenario.observation.composeChildrenCalls == 1);
    assert(scenario.trackedAlive == aliveWhileShown);

    g_componentHostCondition = 0;
  }
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint(
      "testComponentParkedReentryKeepsSubtreeSingular");
#endif
}

void testComponentKeySwapRetiresResidentsTwoPhase()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  int aliveAfterMount = 0;
  {
    ComponentScenario scenario;
    SceneTestSupport::RecordingPlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<ComponentHostRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    ComponentHostRootNode *root = static_cast<ComponentHostRootNode *>(
        loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    assert(root);
    TestCellComponentNode *original = root->component(6001);
    (void)original;
    assert(original);
    assert(scenario.trackedAlive > 0);
    aliveAfterMount = scenario.trackedAlive;
    (void)aliveAfterMount;

    // Identity change: a new Section key retires the old box wholesale and
    // materializes a fresh one. Old residents stay touchable until the
    // drain (two-phase retirement), so both instances are alive here.
    root->setKey(6002);
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());

    TestCellComponentNode *fresh = root->component(6002);
    (void)fresh;
    assert(fresh && fresh != original);
    assert(root->section(6001) == 0);
    assert(scene.hasPendingInvalidation());
    assert(scenario.trackedAlive == 2 * aliveAfterMount &&
           "retired component residents must remain touchable until the drain");
    assert(scenario.observation.composeChildrenCalls == 2);
    assert(scenario.observation.statesValidAtComposeChildren == 2);

    LOKA_VERIFY(!scene.flushInvalidation() &&
                "component retirement must be a silent drain-only run");
    assert(scenario.trackedAlive == aliveAfterMount &&
           "the retired component must release its residents at the drain");
  }
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint(
      "testComponentKeySwapRetiresResidentsTwoPhase");
#endif
}
