#include "LifecycleFactTests.hpp"

#include <cassert>
#include <vector>

#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/Conditional.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "core/State.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/RecomposingBoundary.hpp"

// Shadow-mode pins for the lifecycle fact (S1): the enum is written through
// the single door at the three door sites while delivery still rides the old
// hooks. The probes record which fact a context observes at hook time — the
// distinction the S2 slices will deliver for real (retained detach reads
// DETACHED_RETAINED, terminal detach reads RETIRED).

namespace
{
  struct FactRecord
  {
    FactRecord()
        : node(0),
          terminalContextSevered(false)
    {
    }

    loka::app::scene::Node *node;
    std::vector<loka::app::scene::NodeLifecycleFact> attachFacts;
    std::vector<loka::app::scene::NodeLifecycleFact> detachFacts;
    // G5: at terminal delivery the context is already severed from the node.
    bool terminalContextSevered;
  };

  class FactProbeContext : public loka::app::scene::NodeContext
  {
  public:
    explicit FactProbeContext(FactRecord *record)
        : record_(record)
    {
    }

    /** Attach-time read: the announce successor. */
    void readLifecycleFactOnAttach()
    {
      if (this->owner() && this->record_ &&
          this->owner()->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
      {
        this->record_->attachFacts.push_back(this->owner()->lifecycleFact());
      }
    }

    // Living transitions (S2a) and the terminal (S2b) arrive through the
    // fact channel.
    virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                               loka::app::scene::NodeLifecycleFact next)
    {
      (void)previous;
      if (!this->record_)
      {
        return;
      }
      if (next == loka::app::scene::NODE_FACT_ATTACHED)
      {
        this->record_->attachFacts.push_back(next);
        return;
      }
      this->record_->detachFacts.push_back(next);
      if (next == loka::app::scene::NODE_FACT_RETIRED && this->owner())
      {
        this->record_->terminalContextSevered = this->owner()->getContext() == 0;
      }
    }

  private:
    FactRecord *record_;
  };

  class FactProbeNode;

  struct FactProbeTypeTag
  {
  };

  struct FactProbeProps : public loka::app::scene::NodePropsBase<FactProbeProps>
  {
    typedef FactProbeTypeTag TypeTag;
    typedef FactProbeNode NodeType;

    explicit FactProbeProps(FactRecord *record = 0)
        : record(record)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }

    FactRecord *record;
  };

  class FactProbeNode : public loka::app::scene::Node
  {
  public:
    typedef FactProbeTypeTag TypeTag;

    explicit FactProbeNode(const FactProbeProps &props)
        : props(props)
    {
      if (props.record)
      {
        props.record->node = this;
      }
      FactProbeContext *context = new FactProbeContext(props.record);
      this->setContext(context);
      context->readLifecycleFactOnAttach();
    }

    FactProbeProps props;
  };

  typedef loka::app::scene::NodeDefinition<FactProbeProps, FactProbeNode> FactProbeDefinition;

  class NoopPlatformController : public loka::app::scene::IPlatformController
  {
  public:
    virtual void onChange(loka::app::scene::Node *, loka::app::scene::NodeDirtyFlags, bool) {}
    virtual void synchronize() {}
    virtual bool hasPendingSync() const
    {
      return false;
    }
    virtual void destroy() {}
  };

  class ReleaseTableProbeContext;

  struct ReleaseTableRecord
  {
    ReleaseTableRecord()
        : node(0),
          context(0),
          contextDestroyCount(0)
    {
    }

    loka::app::scene::Node *node;
    ReleaseTableProbeContext *context;
    unsigned contextDestroyCount;
  };

  class ReleaseTableProbeContext : public loka::app::scene::NodeContext
  {
  public:
    explicit ReleaseTableProbeContext(ReleaseTableRecord *record)
        : record_(record),
          marker_(47),
          text_(loka::core::String::Literal("retiring text"))
    {
      if (record_)
      {
        record_->context = this;
      }
    }

    virtual ~ReleaseTableProbeContext()
    {
      if (record_)
      {
        ++record_->contextDestroyCount;
      }
    }

    int marker() const
    {
      return marker_;
    }

    virtual loka::core::State<loka::core::String> *projectedTextState()
    {
      return &text_;
    }

  private:
    ReleaseTableRecord *record_;
    int marker_;
    loka::core::MutableState<loka::core::String> text_;
  };

  class ReleaseTableProbeNode;

  struct ReleaseTableProbeTypeTag
  {
  };

  struct ReleaseTableProbeProps : public loka::app::scene::NodePropsBase<ReleaseTableProbeProps>
  {
    typedef ReleaseTableProbeTypeTag TypeTag;
    typedef ReleaseTableProbeNode NodeType;

    explicit ReleaseTableProbeProps(ReleaseTableRecord *record = 0)
        : record(record)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }

    ReleaseTableRecord *record;
  };

  class ReleaseTableProbeNode : public loka::app::scene::Node
  {
  public:
    typedef ReleaseTableProbeTypeTag TypeTag;

    explicit ReleaseTableProbeNode(const ReleaseTableProbeProps &props)
        : props(props)
    {
      if (props.record)
      {
        props.record->node = this;
      }
      this->setContext(new ReleaseTableProbeContext(props.record));
    }

    ReleaseTableProbeProps props;
  };

  typedef loka::app::scene::NodeDefinition<ReleaseTableProbeProps, ReleaseTableProbeNode>
      ReleaseTableProbeDefinition;

  class ReleaseTablePlatformController : public loka::app::scene::IPlatformController
  {
  public:
    struct Hit
    {
      explicit Hit(ReleaseTableProbeContext *nodeContext)
          : context(nodeContext)
      {
      }

      ReleaseTableProbeContext *context;
    };

    struct BoundCallback
    {
      explicit BoundCallback(loka::core::State<loka::core::String> *boundState)
          : state(boundState)
      {
      }

      loka::core::State<loka::core::String> *state;
    };

    ReleaseTablePlatformController()
        : boundStateReadCount_(0),
          hitReadTotal_(0)
    {
    }

    virtual void onChange(loka::app::scene::Node *, loka::app::scene::NodeDirtyFlags, bool) {}
    virtual void synchronize() {}
    virtual bool hasPendingSync() const
    {
      return false;
    }
    virtual void destroy() {}

    void record(ReleaseTableProbeContext *context)
    {
      assert(context);
      hits_.push_back(Hit(context));
      callbacks_.push_back(BoundCallback(context->projectedTextState()));
    }

    void dispatchSyntheticStateChange()
    {
      for (std::size_t i = 0; i < callbacks_.size(); ++i)
      {
        StateChangedThunk(&callbacks_[i], &boundStateReadCount_);
      }
    }

    void dispatchSyntheticClick()
    {
      for (std::size_t i = 0; i < hits_.size(); ++i)
      {
        hitReadTotal_ += hits_[i].context->marker();
      }
    }

    unsigned boundStateReadCount() const
    {
      return boundStateReadCount_;
    }

    int hitReadTotal() const
    {
      return hitReadTotal_;
    }

    std::size_t hitCount() const
    {
      return hits_.size();
    }

    std::size_t callbackCount() const
    {
      return callbacks_.size();
    }

    virtual void releaseNodeContexts(loka::app::scene::Node *node)
    {
      if (!node)
      {
        return;
      }
      for (unsigned i = 0; loka::app::scene::Node *branch = node->retainedLifecycleBranch(i); ++i)
      {
        this->releaseNodeContexts(branch);
      }
      loka::app::scene::INestable *nestable = node->asNestable();
      if (nestable)
      {
        for (loka::app::scene::Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          this->releaseNodeContexts(child);
        }
      }

      loka::app::scene::NodeContext *context = node->getContext();
      if (context)
      {
        for (std::size_t i = 0; i < hits_.size();)
        {
          if (static_cast<loka::app::scene::NodeContext *>(hits_[i].context) == context)
          {
            hits_.erase(hits_.begin() + i);
          }
          else
          {
            ++i;
          }
        }
        loka::core::State<loka::core::String> *text = context->projectedTextState();
        for (std::size_t i = 0; i < callbacks_.size();)
        {
          if (callbacks_[i].state == text)
          {
            callbacks_.erase(callbacks_.begin() + i);
          }
          else
          {
            ++i;
          }
        }
      }
      node->setContext(0);
    }

  private:
    static void StateChangedThunk(BoundCallback *callback, unsigned *readCount)
    {
      assert(callback && callback->state && readCount);
      if (!callback->state->get().empty())
      {
        ++*readCount;
      }
    }

    std::vector<Hit> hits_;
    std::vector<BoundCallback> callbacks_;
    unsigned boundStateReadCount_;
    int hitReadTotal_;
  };

  ReleaseTableRecord *g_releaseTableRecord = 0;
  loka::core::MutableState<bool> *g_releaseTableVisible = 0;

  class ReleaseTableBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ReleaseTableBoundaryNode> ReleaseTableBoundaryProps;

  class ReleaseTableBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<ReleaseTableBoundaryNode, ReleaseTableBoundaryProps>
  {
  public:
    explicit ReleaseTableBoundaryNode(const ReleaseTableBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<ReleaseTableBoundaryNode, ReleaseTableBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_releaseTableVisible)
      {
        registrar.markDirtyOnChange(g_releaseTableVisible, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_releaseTableVisible && g_releaseTableVisible->get())
      {
        root << ReleaseTableProbeDefinition((ReleaseTableProbeProps(g_releaseTableRecord)));
      }
      composition.declare(root);
    }
  };

  FactRecord *g_swapRecord = 0;
  loka::core::MutableState<bool> *g_swapCondition = 0;

  class SwapProbeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<SwapProbeBoundaryNode> SwapProbeBoundaryProps;

  class SwapProbeBoundaryNode : public loka::app::scene::BoundaryNodeFor<SwapProbeBoundaryNode>
  {
  public:
    explicit SwapProbeBoundaryNode(const SwapProbeBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<SwapProbeBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      FactProbeDefinition probe((FactProbeProps(g_swapRecord)));
      loka::app::FragmentDefinition empty;
      loka::app::scene::ConditionalDefinition conditional(
          (loka::app::scene::ConditionalProps(g_swapCondition, &probe, &empty)));
      loka::app::FragmentDefinition root;
      root << conditional;
      composition.declare(root);
    }
  };

  class DeferredSwapProbeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<DeferredSwapProbeBoundaryNode>
      DeferredSwapProbeBoundaryProps;
  class DeferredSwapProbeBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<DeferredSwapProbeBoundaryNode>
  {
  public:
    explicit DeferredSwapProbeBoundaryNode(const DeferredSwapProbeBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<DeferredSwapProbeBoundaryNode>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      FactProbeDefinition probe((FactProbeProps(g_swapRecord)));
      loka::app::FragmentDefinition empty;
      loka::app::scene::ConditionalDefinition conditional(
          (loka::app::scene::ConditionalProps(g_swapCondition, &probe, &empty)));
      loka::app::FragmentDefinition root;
      root << conditional;
      composition.declare(root);
    }
  };

  FactRecord *g_retireRecord = 0;
  loka::core::MutableState<bool> *g_retireVisible = 0;

  class RetireProbeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RetireProbeBoundaryNode> RetireProbeBoundaryProps;

  class RetireProbeBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<RetireProbeBoundaryNode, RetireProbeBoundaryProps>
  {
  public:
    explicit RetireProbeBoundaryNode(const RetireProbeBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<RetireProbeBoundaryNode, RetireProbeBoundaryProps>(props)
    {
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      if (g_retireVisible)
      {
        registrar.markDirtyOnChange(g_retireVisible, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::FragmentDefinition root;
      if (g_retireVisible && g_retireVisible->get())
      {
        root << FactProbeDefinition((FactProbeProps(g_retireRecord)));
      }
      composition.declare(root);
    }
  };

  FactRecord *g_hiddenRecord = 0;
  loka::core::MutableState<bool> *g_hiddenAncestorVisible = 0;
  loka::core::MutableState<bool> *g_hiddenInnerCondition = 0;

  class HiddenAdoptBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HiddenAdoptBoundaryNode> HiddenAdoptBoundaryProps;

  class HiddenAdoptBoundaryNode : public loka::app::scene::BoundaryNodeFor<HiddenAdoptBoundaryNode>
  {
  public:
    explicit HiddenAdoptBoundaryNode(const HiddenAdoptBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<HiddenAdoptBoundaryNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      FactProbeDefinition probe((FactProbeProps(g_hiddenRecord)));
      loka::app::FragmentDefinition empty;
      loka::app::scene::ConditionalDefinition inner(
          (loka::app::scene::ConditionalProps(g_hiddenInnerCondition, &probe, &empty)));
      loka::app::ShowDefinition outer(g_hiddenAncestorVisible);
      outer << inner;
      loka::app::FragmentDefinition root;
      root << outer;
      composition.declare(root);
    }
  };

  void pumpChild(loka::app::scene::Scene &scene)
  {
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    scene.flushInvalidation();
  }
} // namespace

void testLifecycleFactBornAttachedAndSwapWritesRetainedDetach()
{
  FactRecord record;
  loka::core::MutableState<bool> condition(true);
  g_swapRecord = &record;
  g_swapCondition = &condition;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<SwapProbeBoundaryNode>()));
  NoopPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  assert(record.node && "the probe must be materialized on attach");
  assert(record.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "nodes are born ATTACHED and stay so on the attached path");
  assert(record.attachFacts.size() == 1);
  assert(record.attachFacts[0] == loka::app::scene::NODE_FACT_ATTACHED);

  condition.set(false);
  assert(record.node->lifecycleFact() == loka::app::scene::NODE_FACT_DETACHED_RETAINED &&
         "a retained detach writes DETACHED_RETAINED through the walk door");
  assert(record.detachFacts.size() == 1 &&
         "the immediate-flush floor runs apply (and delivery) within the set");
  assert(record.detachFacts[0] == loka::app::scene::NODE_FACT_DETACHED_RETAINED &&
         "the context observes the retained fact, not a terminal one");

  condition.set(true);
  assert(record.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "re-entry restores ATTACHED");
  assert(record.attachFacts.size() == 2);
  assert(record.attachFacts[1] == loka::app::scene::NODE_FACT_ATTACHED);

  scene.unmount();
  g_swapRecord = 0;
  g_swapCondition = 0;
}

void testLifecycleFactTerminalDetachObservesRetired()
{
  FactRecord record;
  loka::core::MutableState<bool> condition(true);
  g_swapRecord = &record;
  g_swapCondition = &condition;
  {
    loka::app::scene::Scene scene((loka::app::scene::Boundary<SwapProbeBoundaryNode>()));
    NoopPlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(record.detachFacts.empty());

    scene.unmount();
  }
  assert(record.detachFacts.size() == 1);
  assert(record.detachFacts[0] == loka::app::scene::NODE_FACT_RETIRED &&
         "teardown marks the subtree RETIRED before contexts are peeled");
  assert(record.terminalContextSevered &&
         "terminal delivery happens after severing: the context is already off the node (G5)");
  g_swapRecord = 0;
  g_swapCondition = 0;
}

void testLifecycleFactCompositionRetireObservesRetired()
{
  FactRecord record;
  loka::core::MutableState<bool> visible(true);
  g_retireRecord = &record;
  g_retireVisible = &visible;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<RetireProbeBoundaryNode>()));
  NoopPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(record.attachFacts.size() == 1);

  visible.set(false);
  pumpChild(scene);

  assert(record.detachFacts.size() == 1);
  assert(record.detachFacts[0] == loka::app::scene::NODE_FACT_RETIRED &&
         "a composition retire writes RETIRED before releaseNodeContexts peels the probe");

  scene.unmount();
  g_retireRecord = 0;
  g_retireVisible = 0;
}

void testPlatformControllerReleaseDropsRetiredNodeHitEntriesAndBindings()
{
  // This is an isomorphic contract/algorithm pin. The real Toolbox controller
  // needs Retro68 compile coverage and a MAME/Flow runtime follow-up.
  ReleaseTableRecord record;
  loka::core::MutableState<bool> visible(true);
  g_releaseTableRecord = &record;
  g_releaseTableVisible = &visible;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<ReleaseTableBoundaryNode>()));
  ReleaseTablePlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  assert(record.node && record.context);
  platform.record(record.context);
  assert(platform.hitCount() == 1 && platform.callbackCount() == 1);

  visible.set(false);
  pumpChild(scene);

  assert(record.contextDestroyCount == 1 &&
         "the real retirement path deletes the per-node context before the next render");
  // Touch the recorded raw pointers after the per-node context was freed. With the
  // override, releaseNodeContexts already dropped them so these loops are empty and
  // safe; without it they read the freed context/state (ASan heap-use-after-free).
  platform.dispatchSyntheticStateChange();
  platform.dispatchSyntheticClick();
  assert(platform.hitCount() == 0 && platform.callbackCount() == 0 &&
         "releaseNodeContexts must drop controller-side raw pointers at the detach line");
  assert(platform.boundStateReadCount() == 0 && platform.hitReadTotal() == 0);

  scene.unmount();
  g_releaseTableRecord = 0;
  g_releaseTableVisible = 0;
}

void testLifecycleFactWalkIsSilentAndDeliveryIsDiffBased()
{
  FactRecord record;
  loka::core::MutableState<bool> condition(true);
  g_swapRecord = &record;
  g_swapCondition = &condition;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<DeferredSwapProbeBoundaryNode>()));
  NoopPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(record.node && record.attachFacts.size() == 1);

  condition.set(false);
  assert(scene.hasPendingInvalidation());
  assert(record.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         record.detachFacts.empty() &&
         "a condition write only marks the real boundary door");

  condition.set(true);
  pumpChild(scene);
  assert(record.detachFacts.empty() && record.attachFacts.size() == 1 &&
         "a round trip before one real delivery remains silent");

  condition.set(false);
  pumpChild(scene);
  assert(record.detachFacts.size() == 1 &&
         record.detachFacts[0] == loka::app::scene::NODE_FACT_DETACHED_RETAINED &&
         "a one-way scheduled apply delivers exactly once");

  scene.unmount();
  g_swapRecord = 0;
  g_swapCondition = 0;
}

void testLifecycleFactChildAdoptedUnderHiddenAncestorInheritsDetached()
{
  FactRecord record;
  loka::core::MutableState<bool> ancestorVisible(true);
  loka::core::MutableState<bool> innerCondition(false);
  g_hiddenRecord = &record;
  g_hiddenAncestorVisible = &ancestorVisible;
  g_hiddenInnerCondition = &innerCondition;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<HiddenAdoptBoundaryNode>()));
  NoopPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(!record.node && "the probe branch must not exist before the inner condition flips");

  ancestorVisible.set(false);
  pumpChild(scene);
  innerCondition.set(true);

  assert(!record.node &&
         "a condition write does not materialize a branch outside the scheduled walk");

  ancestorVisible.set(true);
  pumpChild(scene);

  assert(record.node && "the scheduled walk materializes the current branch on re-entry");
  assert(record.node->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED &&
         "the ancestor's re-attach walk restores ATTACHED for the then-active path");

  scene.unmount();
  g_hiddenRecord = 0;
  g_hiddenAncestorVisible = 0;
  g_hiddenInnerCondition = 0;
}
