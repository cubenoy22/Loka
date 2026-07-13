#include "LifecycleDetachTests.hpp"

#include <cassert>

#include "app/nodes/Text.hpp"
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
}
