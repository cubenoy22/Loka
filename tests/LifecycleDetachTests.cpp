#include "LifecycleDetachTests.hpp"

#include <cassert>

#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/context/ComponentContext.hpp"
#include "app/scene/node/Conditional.hpp"
#include "app/scene/projection/PlatformController.hpp"

namespace
{
  struct DetachHookCounts
  {
    DetachHookCounts()
        : attachCalls(0),
          detachCalls(0)
    {
    }

    int attachCalls;
    int detachCalls;
  };

  class PlainDetachProbeContext : public loka::app::scene::NodeContext
  {
  public:
    explicit PlainDetachProbeContext(DetachHookCounts *counts)
        : counts_(counts)
    {
    }

    virtual void onNodeAttached()
    {
      ++this->counts_->attachCalls;
    }

    virtual void onNodeDetached()
    {
      ++this->counts_->detachCalls;
    }

  private:
    DetachHookCounts *counts_;
  };

  class PlainDetachProbeNode;

  struct PlainDetachProbeTypeTag
  {
  };

  struct PlainDetachProbeProps : public loka::app::scene::NodePropsBase<PlainDetachProbeProps>
  {
    typedef PlainDetachProbeTypeTag TypeTag;
    typedef PlainDetachProbeNode NodeType;

    explicit PlainDetachProbeProps(DetachHookCounts *counts = 0)
        : counts(counts)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }

    DetachHookCounts *counts;
  };

  class PlainDetachProbeNode : public loka::app::scene::Node
  {
  public:
    typedef PlainDetachProbeTypeTag TypeTag;

    explicit PlainDetachProbeNode(const PlainDetachProbeProps &props)
        : props(props)
    {
      this->setContext(new PlainDetachProbeContext(props.counts));
    }

    PlainDetachProbeProps props;
  };

  class CompositionDetachProbeNode : public loka::app::scene::Node
  {
  public:
    explicit CompositionDetachProbeNode(int *detachCalls)
        : detachCalls_(detachCalls)
    {
    }

    virtual void onCompositionDetached()
    {
      ++*this->detachCalls_;
    }

  private:
    int *detachCalls_;
  };

  DetachHookCounts *g_boundaryInternalCounts = 0;

  DetachHookCounts *g_conditionalSceneTrueCounts = 0;
  DetachHookCounts *g_conditionalSceneFalseCounts = 0;
  loka::core::MutableState<bool> *g_conditionalSceneCondition = 0;

  class ConditionalSceneProbeNode;
  typedef loka::app::scene::BoundaryPropsFor<ConditionalSceneProbeNode> ConditionalSceneProbeProps;

  class ConditionalSceneProbeNode : public loka::app::scene::BoundaryNodeFor<ConditionalSceneProbeNode>
  {
  public:
    explicit ConditionalSceneProbeNode(const ConditionalSceneProbeProps &props)
        : loka::app::scene::BoundaryNodeFor<ConditionalSceneProbeNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      typedef loka::app::scene::NodeDefinition<PlainDetachProbeProps, PlainDetachProbeNode>
          PlainDetachProbeDefinition;
      PlainDetachProbeDefinition trueDefinition((PlainDetachProbeProps(g_conditionalSceneTrueCounts)));
      PlainDetachProbeDefinition falseDefinition((PlainDetachProbeProps(g_conditionalSceneFalseCounts)));
      composition.declare(
          composition.conditional(*g_conditionalSceneCondition, trueDefinition, falseDefinition));
    }
  };

  class BoundaryInternalProbeNode;
  typedef loka::app::scene::BoundaryPropsFor<BoundaryInternalProbeNode> BoundaryInternalProbeProps;

  class BoundaryInternalProbeNode : public loka::app::scene::BoundaryNodeFor<BoundaryInternalProbeNode>
  {
  public:
    explicit BoundaryInternalProbeNode(const BoundaryInternalProbeProps &props)
        : loka::app::scene::BoundaryNodeFor<BoundaryInternalProbeNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      typedef loka::app::scene::NodeDefinition<PlainDetachProbeProps, PlainDetachProbeNode>
          PlainDetachProbeDefinition;
      composition.declare(PlainDetachProbeDefinition((PlainDetachProbeProps(g_boundaryInternalCounts))));
    }
  };

  class LocalRebuildProbeBoundary : public loka::app::scene::BoundaryNode
  {
  public:
    bool applyProbeLocalRebuildPlan(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::INestable &root,
                                    loka::app::scene::BoundaryLocalRebuildPlan &plan,
                                    std::vector<loka::app::scene::Node *> &retainedChildren)
    {
      return this->applyLocalRebuildPlan(context, root, plan, retainedChildren);
    }

  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &, loka::app::scene::ComposeEvent) {}
  };

  struct DetachProbePlatformController : public loka::app::scene::IPlatformController
  {
    virtual void onChange(loka::app::scene::Node *, loka::app::scene::NodeDirtyFlags, bool) {}
    virtual void synchronize() {}
    virtual bool hasPendingSync() const
    {
      return false;
    }
    virtual void destroy() {}
  };
} // namespace

void testSceneUnmountNotifiesPlainNodeContextDetached()
{
  typedef loka::app::scene::NodeDefinition<PlainDetachProbeProps, PlainDetachProbeNode> PlainDetachProbeDefinition;

  DetachHookCounts counts;
  PlainDetachProbeDefinition definition((PlainDetachProbeProps(&counts)));
  loka::app::scene::Scene scene(definition.clone());
  DetachProbePlatformController platform;

  scene.mount(&platform);
  scene.updateAttached(true);
  assert(counts.detachCalls == 0);

  scene.unmount();

  assert(counts.detachCalls == 1);
}

void testSceneUpdateAttachedFalseNotifiesPlainNodeContextDetachedOnce()
{
  typedef loka::app::scene::NodeDefinition<PlainDetachProbeProps, PlainDetachProbeNode> PlainDetachProbeDefinition;

  DetachHookCounts counts;
  PlainDetachProbeDefinition definition((PlainDetachProbeProps(&counts)));
  loka::app::scene::Scene scene(definition.clone());
  DetachProbePlatformController platform;

  scene.mount(&platform);
  scene.updateAttached(true);
  assert(counts.attachCalls == 1);
  assert(counts.detachCalls == 0);

  scene.updateAttached(false);

  assert(counts.detachCalls == 1);
}

void testBoundaryLocalRebuildNotifiesCompositionDetachedOnce()
{
  using namespace loka::app::scene;

  int detachCalls = 0;
  NestableNode root;
  CompositionDetachProbeNode *probe = new CompositionDetachProbeNode(&detachCalls);
  root.addChild(probe);

  BoundaryLocalRebuildPlan plan;
  plan.entries.push_back(BoundaryLocalRebuildPlanEntry::retire(probe, NODE_TAG_NONE));
  std::vector<Node *> retainedChildren;
  ComponentContext context;
  LocalRebuildProbeBoundary boundary;

  assert(boundary.applyProbeLocalRebuildPlan(context, root, plan, retainedChildren));
  assert(detachCalls == 1);
}

void testSceneTeardownNotifiesBoundaryInternalNodeContextDetachedOnce()
{
  using namespace loka::app::scene;

  DetachHookCounts counts;
  g_boundaryInternalCounts = &counts;
  Scene scene((Boundary<BoundaryInternalProbeNode>()));
  DetachProbePlatformController platform;

  scene.mount(&platform);
  scene.updateAttached(true);
  assert(counts.attachCalls == 1);
  assert(counts.detachCalls == 0);

  scene.updateAttached(false);

  assert(counts.detachCalls == 1);
  g_boundaryInternalCounts = 0;
}

void testConditionalUnbindsBeforeReclaim()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  loka::core::MutableState<bool> condition(false);
  TextDefinition falseDefinition = Text("Detached false branch");
  TextDefinition trueDefinition = Text("Detached true branch");
  ConditionalNode conditional((ConditionalProps(&condition, &trueDefinition, &falseDefinition)));
  Node *activeBeforeDetach = conditional.activeNode;
  ComponentContext context;

  BoundaryNode::composeSubtree(&conditional, context, COMPOSE_EVENT_DETACH, 0);
  condition.set(true);

  assert(conditional.activeNode == activeBeforeDetach);

  BoundaryNode::composeSubtree(&conditional, context, COMPOSE_EVENT_ATTACH, 0);
  assert(conditional.activeNode != activeBeforeDetach);

  condition.set(false);
  assert(conditional.activeNode == activeBeforeDetach);
}

void testConditionalBranchSwapFiresContextHooksOncePerLifetime()
{
  using namespace loka::app::scene;
  typedef NodeDefinition<PlainDetachProbeProps, PlainDetachProbeNode> PlainDetachProbeDefinition;

  DetachHookCounts trueCounts;
  DetachHookCounts falseCounts;
  loka::core::MutableState<bool> condition(false);
  PlainDetachProbeDefinition trueDefinition((PlainDetachProbeProps(&trueCounts)));
  PlainDetachProbeDefinition falseDefinition((PlainDetachProbeProps(&falseCounts)));

  {
    ConditionalNode conditional((ConditionalProps(&condition, &trueDefinition, &falseDefinition)));
    assert(falseCounts.attachCalls == 1);
    assert(trueCounts.attachCalls == 0);

    condition.set(true);
    assert(trueCounts.attachCalls == 1);
    assert(falseCounts.detachCalls == 0);

    condition.set(false);
    assert(falseCounts.attachCalls == 1);
    assert(trueCounts.detachCalls == 0);
  }

  assert(trueCounts.attachCalls == 1);
  assert(falseCounts.attachCalls == 1);
  assert(trueCounts.detachCalls == 1);
  assert(falseCounts.detachCalls == 1);
}

void testSceneTeardownReleasesBothConditionalBranchContextsOnce()
{
  using namespace loka::app::scene;

  DetachHookCounts trueCounts;
  DetachHookCounts falseCounts;
  loka::core::MutableState<bool> condition(false);
  g_conditionalSceneTrueCounts = &trueCounts;
  g_conditionalSceneFalseCounts = &falseCounts;
  g_conditionalSceneCondition = &condition;

  {
    Scene scene((Boundary<ConditionalSceneProbeNode>()));
    DetachProbePlatformController platform;

    scene.mount(&platform);
    scene.updateAttached(true);
    assert(falseCounts.attachCalls == 1);
    assert(trueCounts.attachCalls == 0);

    condition.set(true);
    assert(trueCounts.attachCalls == 1);
    assert(falseCounts.detachCalls == 0);

    scene.unmount();

    assert(trueCounts.detachCalls == 1);
    assert(falseCounts.detachCalls == 1);
    assert(trueCounts.attachCalls == 1);
    assert(falseCounts.attachCalls == 1);
  }

  g_conditionalSceneTrueCounts = 0;
  g_conditionalSceneFalseCounts = 0;
  g_conditionalSceneCondition = 0;
}
