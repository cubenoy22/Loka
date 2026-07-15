#include "LifecycleDetachTests.hpp"

#include <cassert>

#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Fragment.hpp"
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

  struct DetachStateOwnerObservation
  {
    DetachStateOwnerObservation()
        : detachCalls(0),
          currentBoundaryValid(false),
          parentStateReadable(false),
          parentStateValue(0)
    {
    }

    int detachCalls;
    bool currentBoundaryValid;
    bool parentStateReadable;
    int parentStateValue;
  };

  DetachStateOwnerObservation *g_detachStateOwnerObservation = 0;
  loka::app::scene::NodeState<int> *g_detachParentState = 0;

  class DetachStateOwnerProbeNode;

  struct DetachStateOwnerProbeTypeTag
  {
  };

  struct DetachStateOwnerProbeProps
      : public loka::app::scene::NodePropsBase<DetachStateOwnerProbeProps>
  {
    typedef DetachStateOwnerProbeTypeTag TypeTag;
    typedef DetachStateOwnerProbeNode NodeType;

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
  };

  class DetachStateOwnerProbeNode : public loka::app::scene::ComposableNode
  {
  public:
    typedef DetachStateOwnerProbeTypeTag TypeTag;

    explicit DetachStateOwnerProbeNode(const DetachStateOwnerProbeProps &props)
        : props(props)
    {
    }

    DetachStateOwnerProbeProps props;

  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      if (event == loka::app::scene::COMPOSE_EVENT_DETACH)
      {
        loka::app::scene::NodeComposition &composition = this->beginComposition(context);
        this->detachNode(composition);
      }
    }

    virtual void detachNode(loka::app::scene::NodeComposition &composition)
    {
      typedef loka::app::scene::NodeComposition::CurrentBoundary CurrentBoundary;
      typedef CurrentBoundary::CurrentState<int> CurrentState;

      assert(g_detachStateOwnerObservation != 0);
      assert(g_detachParentState != 0);
      ++g_detachStateOwnerObservation->detachCalls;
      const CurrentBoundary current = composition.currentBoundary();
      g_detachStateOwnerObservation->currentBoundaryValid = current.isValid();
      const CurrentState parentState = current.state(*g_detachParentState);
      g_detachStateOwnerObservation->parentStateReadable = parentState.isValid();
      if (parentState.isValid())
      {
        g_detachStateOwnerObservation->parentStateValue = parentState.get();
      }
    }
  };

  class RootDetachStateOwnerBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RootDetachStateOwnerBoundaryNode>
      RootDetachStateOwnerBoundaryProps;

  class RootDetachStateOwnerBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<RootDetachStateOwnerBoundaryNode>
  {
  public:
    explicit RootDetachStateOwnerBoundaryNode(const RootDetachStateOwnerBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<RootDetachStateOwnerBoundaryNode>(props),
          parentState_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      if (this->initialized_)
      {
        return;
      }
      composition.declareStates().state(this->parentState_, 37);
      g_detachParentState = &this->parentState_;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      typedef loka::app::scene::NodeDefinition<DetachStateOwnerProbeProps,
                                               DetachStateOwnerProbeNode>
          DetachStateOwnerProbeDefinition;
      composition.declare(DetachStateOwnerProbeDefinition());
    }

  private:
    loka::app::scene::NodeState<int> parentState_;
    bool initialized_;
  };

  class NestedDetachStateOwnerBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<NestedDetachStateOwnerBoundaryNode>
      NestedDetachStateOwnerBoundaryProps;

  class NestedDetachStateOwnerBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<NestedDetachStateOwnerBoundaryNode>
  {
  public:
    explicit NestedDetachStateOwnerBoundaryNode(const NestedDetachStateOwnerBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<NestedDetachStateOwnerBoundaryNode>(props),
          parentState_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      if (this->initialized_)
      {
        return;
      }
      composition.declareStates().state(this->parentState_, 73);
      g_detachParentState = &this->parentState_;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      typedef loka::app::scene::NodeDefinition<DetachStateOwnerProbeProps,
                                               DetachStateOwnerProbeNode>
          DetachStateOwnerProbeDefinition;
      composition.declare(DetachStateOwnerProbeDefinition());
    }

  private:
    loka::app::scene::NodeState<int> parentState_;
    bool initialized_;
  };

  class NestedDetachStateOwnerHarnessNode;
  typedef loka::app::scene::BoundaryPropsFor<NestedDetachStateOwnerHarnessNode>
      NestedDetachStateOwnerHarnessProps;

  class NestedDetachStateOwnerHarnessNode
      : public loka::app::scene::BoundaryNodeFor<NestedDetachStateOwnerHarnessNode>
  {
  public:
    explicit NestedDetachStateOwnerHarnessNode(const NestedDetachStateOwnerHarnessProps &props)
        : loka::app::scene::BoundaryNodeFor<NestedDetachStateOwnerHarnessNode>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::scene::Boundary<NestedDetachStateOwnerBoundaryNode>());
    }
  };

  class ArenaRetireProbeNode;

  struct ArenaRetireProbeTypeTag
  {
  };

  struct ArenaRetireProbeProps : public loka::app::scene::NodePropsBase<ArenaRetireProbeProps>
  {
    typedef ArenaRetireProbeTypeTag TypeTag;
    typedef ArenaRetireProbeNode NodeType;

    explicit ArenaRetireProbeProps(int *destructorCalls = 0)
        : destructorCalls(destructorCalls)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }

    int *destructorCalls;
  };

  class ArenaRetireProbeNode : public loka::app::scene::Node
  {
  public:
    typedef ArenaRetireProbeTypeTag TypeTag;

    explicit ArenaRetireProbeNode(const ArenaRetireProbeProps &props)
        : props(props)
    {
    }

    virtual ~ArenaRetireProbeNode()
    {
      if (this->props.destructorCalls)
      {
        ++*this->props.destructorCalls;
      }
    }

    ArenaRetireProbeProps props;
  };

  class RootReplacementArenaRetireBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RootReplacementArenaRetireBoundaryNode>
      RootReplacementArenaRetireBoundaryProps;

  int *g_rootReplacementRetireDestructorCalls = 0;
  RootReplacementArenaRetireBoundaryNode *g_rootReplacementArenaRetireBoundary = 0;

  class RootReplacementArenaRetireBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<RootReplacementArenaRetireBoundaryNode>
  {
  public:
    explicit RootReplacementArenaRetireBoundaryNode(const RootReplacementArenaRetireBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<RootReplacementArenaRetireBoundaryNode>(props),
          showReplacement_(),
          initialized_(false)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::BoundaryNodeFor<RootReplacementArenaRetireBoundaryNode> BaseType;
      if (event != loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        BaseType::composeWithContext(context, event);
        return;
      }

      loka::app::scene::NodeComposition &composition = this->beginComposition(context);
      {
        loka::app::scene::NodeComposition::CompositionScope scope(composition);
        this->composeNode(composition);
      }
      this->captureCurrentCompositionSnapshot();
      this->rebuildCurrentCompositionDiff();
      std::vector<loka::app::scene::Node *> retainedChildren;
      if (!this->rebuildCompositionRootFromCurrentSnapshot(context, retainedChildren))
      {
        return;
      }
      this->promoteCurrentCompositionSnapshot();
      for (size_t i = 0; i < retainedChildren.size(); ++i)
      {
        if (retainedChildren[i])
        {
          this->composeSubtree(retainedChildren[i], context, event, this);
        }
      }
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      if (this->initialized_)
      {
        return;
      }
      composition.declareStates().state(this->showReplacement_, false);
      g_rootReplacementArenaRetireBoundary = this;
      this->initialized_ = true;
    }

    virtual void detachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      if (g_rootReplacementArenaRetireBoundary == this)
      {
        g_rootReplacementArenaRetireBoundary = 0;
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      if (this->showReplacement_.get())
      {
        composition.declare(loka::app::Text("Replacement root"));
      }
      else
      {
        typedef loka::app::scene::NodeDefinition<ArenaRetireProbeProps, ArenaRetireProbeNode>
            ArenaRetireProbeDefinition;
        composition.declare(
            ArenaRetireProbeDefinition((ArenaRetireProbeProps(g_rootReplacementRetireDestructorCalls))));
      }
    }

    loka::app::scene::Node *activeRootNode() const
    {
      return this->compositionRootNode();
    }

    void showReplacement()
    {
      this->showReplacement_.set(true, true);
      this->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
    }

  private:
    loka::app::scene::NodeState<bool> showReplacement_;
    bool initialized_;
  };

  class RootUpdateFallbackDefinition : public loka::app::scene::NodeDefinitionBase
  {
  public:
    RootUpdateFallbackDefinition(bool *useAlternate,
                                 loka::app::scene::NodeDefinitionBase *initial,
                                 loka::app::scene::NodeDefinitionBase *alternate)
        : useAlternate_(useAlternate),
          initial_(initial),
          alternate_(alternate)
    {
      assert(this->useAlternate_ != 0);
      assert(this->initial_ != 0);
      assert(this->alternate_ != 0);
    }

    virtual loka::app::scene::Node *create() const
    {
      return this->selected()->create();
    }

    virtual loka::app::scene::Node *createInPlace(void *mem) const
    {
      return this->selected()->createInPlace(mem);
    }

    virtual size_t nodeSize() const
    {
      return this->selected()->nodeSize();
    }

    virtual size_t nodeAlign() const
    {
      return this->selected()->nodeAlign();
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return this->selected()->clone();
    }

    virtual loka::app::scene::NodeKind nodeKind() const
    {
      return this->selected()->nodeKind();
    }

    virtual const loka::app::scene::PropsBase *propsBase() const
    {
      return this->selected()->propsBase();
    }

    virtual bool hasEquivalentProps(const loka::app::scene::NodeDefinitionBase &other) const
    {
      return this->selected()->hasEquivalentProps(other);
    }

    virtual bool applyPropsToNode(loka::app::scene::Node *node) const
    {
      return this->selected()->applyPropsToNode(node);
    }

  private:
    loka::app::scene::NodeDefinitionBase *selected() const
    {
      return *this->useAlternate_ ? this->alternate_ : this->initial_;
    }

    bool *useAlternate_;
    loka::app::scene::NodeDefinitionBase *initial_;
    loka::app::scene::NodeDefinitionBase *alternate_;
  };

  class RootUpdateFallbackTestScene : public loka::app::scene::Scene
  {
  public:
    explicit RootUpdateFallbackTestScene(loka::app::scene::NodeDefinitionBase *definition)
        : loka::app::scene::Scene(definition)
    {
    }

    loka::app::scene::BoundaryNode *rootBoundary() const
    {
      return this->rootNode_ ? this->rootNode_->asBoundary() : 0;
    }
  };

  class RetiredGenerationOverlapBoundary : public loka::app::scene::BoundaryNode
  {
  public:
    void reserveProbeNodes(size_t count)
    {
      const size_t probeBytes = sizeof(ArenaRetireProbeNode)
                                + loka::app::scene::detail::AlignOf<ArenaRetireProbeNode>::value;
      this->nodeArena()->reserve(probeBytes * count);
    }

    loka::app::scene::Node *createProbeNode(int *destructorCalls)
    {
      void *memory = this->nodeArena()->allocate(
          sizeof(ArenaRetireProbeNode),
          loka::app::scene::detail::AlignOf<ArenaRetireProbeNode>::value);
      assert(memory != 0);
      ArenaRetireProbeNode *node =
          new (memory) ArenaRetireProbeNode((ArenaRetireProbeProps(destructorCalls)));
      this->nodeArena()->registerNode(node);
      return node;
    }

    void queueSubtree(loka::app::scene::Node *subtree)
    {
      using namespace loka::app::scene;

      ComponentContext context;
      context.setBoundary(this);
      context.setStateOwner(this);
      NestableNode root;
      root.addChild(subtree);
      BoundaryLocalRebuildPlan plan;
      plan.entries.push_back(
          BoundaryLocalRebuildPlanEntry::retire(subtree, subtree->nodeTag()));
      std::vector<Node *> retainedChildren;
      assert(this->applyLocalRebuildPlan(context, root, plan, retainedChildren));
      assert(retainedChildren.empty());
    }

    void queueSubtreeThenRetireGeneration(loka::app::scene::Node *subtree)
    {
      this->queueSubtree(subtree);
      this->retireOwnedNodeGeneration();
    }

  protected:
    virtual void composeWithContext(loka::app::scene::ComponentContext &,
                                    loka::app::scene::ComposeEvent)
    {
    }
  };

  class ArenaRetireOrderLeafNode;
  struct ArenaRetireOrderLeafTypeTag
  {
  };
  struct ArenaRetireOrderLeafProps : public loka::app::scene::NodePropsBase<ArenaRetireOrderLeafProps>
  {
    typedef ArenaRetireOrderLeafTypeTag TypeTag;
    typedef ArenaRetireOrderLeafNode NodeType;
    ArenaRetireOrderLeafProps(std::vector<int> *order = 0, int marker = 0)
        : order(order), marker(marker) {}
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
    std::vector<int> *order;
    int marker;
  };
  class ArenaRetireOrderLeafNode : public loka::app::scene::Node
  {
  public:
    typedef ArenaRetireOrderLeafTypeTag TypeTag;
    explicit ArenaRetireOrderLeafNode(const ArenaRetireOrderLeafProps &props) : props(props) {}
    virtual ~ArenaRetireOrderLeafNode()
    {
      if (this->props.order)
      {
        this->props.order->push_back(this->props.marker);
      }
    }
    ArenaRetireOrderLeafProps props;
  };

  class ArenaRetireOrderParentNode;
  struct ArenaRetireOrderParentTypeTag
  {
  };
  struct ArenaRetireOrderParentProps : public loka::app::scene::NodePropsBase<ArenaRetireOrderParentProps>
  {
    typedef ArenaRetireOrderParentTypeTag TypeTag;
    typedef ArenaRetireOrderParentNode NodeType;
    ArenaRetireOrderParentProps(std::vector<int> *order = 0, int marker = 0)
        : order(order), marker(marker) {}
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
    std::vector<int> *order;
    int marker;
  };
  class ArenaRetireOrderParentNode : public loka::app::scene::NestableNode
  {
  public:
    typedef ArenaRetireOrderParentTypeTag TypeTag;
    explicit ArenaRetireOrderParentNode(const ArenaRetireOrderParentProps &props) : props(props) {}
    virtual ~ArenaRetireOrderParentNode()
    {
      if (this->props.order)
      {
        this->props.order->push_back(this->props.marker);
      }
    }
    ArenaRetireOrderParentProps props;
  };
  struct ArenaRetireOrderParentDefinition
      : public loka::app::scene::NestableNodeDefinition<ArenaRetireOrderParentProps,
                                                       ArenaRetireOrderParentNode,
                                                       ArenaRetireOrderParentDefinition>
  {
    typedef loka::app::scene::NestableNodeDefinition<ArenaRetireOrderParentProps,
                                                     ArenaRetireOrderParentNode,
                                                     ArenaRetireOrderParentDefinition> BaseType;
    using BaseType::operator<<;
    ArenaRetireOrderParentDefinition() : BaseType() {}
    explicit ArenaRetireOrderParentDefinition(const ArenaRetireOrderParentProps &props) : BaseType(props) {}
    ArenaRetireOrderParentDefinition(const ArenaRetireOrderParentDefinition &other) : BaseType(other) {}
  };

  class HeapRetireChildNode : public loka::app::scene::Node
  {
  public:
    explicit HeapRetireChildNode(int *destructorCalls)
        : destructorCalls_(destructorCalls)
    {
    }

    virtual ~HeapRetireChildNode()
    {
      if (this->destructorCalls_)
      {
        ++*this->destructorCalls_;
      }
    }

  private:
    int *destructorCalls_;
  };

  class ArenaParentWithHeapChildNode;
  struct ArenaParentWithHeapChildTypeTag
  {
  };
  struct ArenaParentWithHeapChildProps
      : public loka::app::scene::NodePropsBase<ArenaParentWithHeapChildProps>
  {
    typedef ArenaParentWithHeapChildTypeTag TypeTag;
    typedef ArenaParentWithHeapChildNode NodeType;
    ArenaParentWithHeapChildProps(int *parentDestructorCalls = 0,
                                  int *childDestructorCalls = 0)
        : parentDestructorCalls(parentDestructorCalls),
          childDestructorCalls(childDestructorCalls)
    {
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
    int *parentDestructorCalls;
    int *childDestructorCalls;
  };
  class ArenaParentWithHeapChildNode : public loka::app::scene::NestableNode
  {
  public:
    typedef ArenaParentWithHeapChildTypeTag TypeTag;
    explicit ArenaParentWithHeapChildNode(const ArenaParentWithHeapChildProps &props)
        : props(props)
    {
      this->addChild(new HeapRetireChildNode(props.childDestructorCalls));
    }
    virtual ~ArenaParentWithHeapChildNode()
    {
      if (this->props.parentDestructorCalls)
      {
        ++*this->props.parentDestructorCalls;
      }
    }
    ArenaParentWithHeapChildProps props;
  };
  struct ArenaParentWithHeapChildDefinition
      : public loka::app::scene::NestableNodeDefinition<ArenaParentWithHeapChildProps,
                                                       ArenaParentWithHeapChildNode,
                                                       ArenaParentWithHeapChildDefinition>
  {
    typedef loka::app::scene::NestableNodeDefinition<ArenaParentWithHeapChildProps,
                                                     ArenaParentWithHeapChildNode,
                                                     ArenaParentWithHeapChildDefinition> BaseType;
    using BaseType::operator<<;
    ArenaParentWithHeapChildDefinition() : BaseType() {}
    explicit ArenaParentWithHeapChildDefinition(const ArenaParentWithHeapChildProps &props) : BaseType(props) {}
    ArenaParentWithHeapChildDefinition(const ArenaParentWithHeapChildDefinition &other) : BaseType(other) {}
  };

  int *g_heapRetireParentDestructorCalls = 0;
  int *g_heapRetireChildDestructorCalls = 0;

  class NestedArenaRetireLeafNode;
  struct NestedArenaRetireLeafTypeTag
  {
  };
  struct NestedArenaRetireLeafProps
      : public loka::app::scene::NodePropsBase<NestedArenaRetireLeafProps>
  {
    typedef NestedArenaRetireLeafTypeTag TypeTag;
    typedef NestedArenaRetireLeafNode NodeType;
    explicit NestedArenaRetireLeafProps(int *destructorCalls = 0)
        : destructorCalls(destructorCalls)
    {
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
    int *destructorCalls;
  };
  class NestedArenaRetireLeafNode : public loka::app::scene::Node
  {
  public:
    typedef NestedArenaRetireLeafTypeTag TypeTag;
    explicit NestedArenaRetireLeafNode(const NestedArenaRetireLeafProps &props)
        : props(props)
    {
    }
    virtual ~NestedArenaRetireLeafNode()
    {
      if (this->props.destructorCalls)
      {
        ++*this->props.destructorCalls;
      }
    }
    NestedArenaRetireLeafProps props;
  };

  class NestedArenaRetireBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<NestedArenaRetireBoundaryNode>
      NestedArenaRetireBoundaryProps;

  int *g_nestedArenaRetireBoundaryDestructorCalls = 0;
  int *g_nestedArenaRetireLeafDestructorCalls = 0;

  class NestedArenaRetireBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<NestedArenaRetireBoundaryNode>
  {
  public:
    explicit NestedArenaRetireBoundaryNode(const NestedArenaRetireBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<NestedArenaRetireBoundaryNode>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      // Keep dirtied instances NextTick-pending so pins can observe the
      // pending-target entry across a structural replacement.
      return false;
    }

    virtual ~NestedArenaRetireBoundaryNode()
    {
      if (g_nestedArenaRetireBoundaryDestructorCalls)
      {
        ++*g_nestedArenaRetireBoundaryDestructorCalls;
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      typedef loka::app::scene::NodeDefinition<NestedArenaRetireLeafProps,
                                               NestedArenaRetireLeafNode>
          NestedArenaRetireLeafDefinition;
      loka::app::Fragment root;
      root << NestedArenaRetireLeafDefinition(
          (NestedArenaRetireLeafProps(g_nestedArenaRetireLeafDestructorCalls)));
      composition.declare(root);
    }

    loka::app::scene::Node *innerArenaChild() const
    {
      loka::app::scene::Node *root = this->compositionRootNode();
      loka::app::scene::INestable *nestable = root ? root->asNestable() : 0;
      return nestable ? nestable->childrenHead() : 0;
    }
  };

  class NestedArenaRetireOuterNode;
  struct NestedArenaRetireOuterTypeTag
  {
  };
  struct NestedArenaRetireOuterProps
      : public loka::app::scene::NodePropsBase<NestedArenaRetireOuterProps>
  {
    typedef NestedArenaRetireOuterTypeTag TypeTag;
    typedef NestedArenaRetireOuterNode NodeType;
    explicit NestedArenaRetireOuterProps(int *destructorCalls = 0)
        : destructorCalls(destructorCalls)
    {
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
    int *destructorCalls;
  };
  class NestedArenaRetireOuterNode : public loka::app::scene::NestableNode
  {
  public:
    typedef NestedArenaRetireOuterTypeTag TypeTag;
    explicit NestedArenaRetireOuterNode(const NestedArenaRetireOuterProps &props)
        : props(props)
    {
    }
    virtual ~NestedArenaRetireOuterNode()
    {
      if (this->props.destructorCalls)
      {
        ++*this->props.destructorCalls;
      }
    }
    NestedArenaRetireOuterProps props;
  };
  struct NestedArenaRetireOuterDefinition
      : public loka::app::scene::NestableNodeDefinition<NestedArenaRetireOuterProps,
                                                       NestedArenaRetireOuterNode,
                                                       NestedArenaRetireOuterDefinition>
  {
    typedef loka::app::scene::NestableNodeDefinition<NestedArenaRetireOuterProps,
                                                     NestedArenaRetireOuterNode,
                                                     NestedArenaRetireOuterDefinition> BaseType;
    using BaseType::operator<<;
    NestedArenaRetireOuterDefinition() : BaseType() {}
    explicit NestedArenaRetireOuterDefinition(const NestedArenaRetireOuterProps &props) : BaseType(props) {}
    NestedArenaRetireOuterDefinition(const NestedArenaRetireOuterDefinition &other) : BaseType(other) {}
  };

  int *g_nestedArenaRetireOuterDestructorCalls = 0;
  bool g_heapReplacementNestedBranch = false;

  class NativeBindingStateBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<NativeBindingStateBoundaryNode>
      NativeBindingStateBoundaryProps;

  NativeBindingStateBoundaryNode *g_nativeBindingStateBoundary = 0;
  bool g_includeNativeBindingRetireBranch = false;
  bool *g_nativeBindingStateNodeDestroyed = 0;

  class NativeBindingStateContext : public loka::app::scene::NodeContext
  {
  public:
    NativeBindingStateContext(loka::core::State<int> *state,
                              bool *unboundWhileStateAlive,
                              const bool *nodeDestroyed = 0)
        : state_(state),
          unboundWhileStateAlive_(unboundWhileStateAlive),
          nodeDestroyed_(nodeDestroyed)
    {
      assert(this->state_ != 0);
      this->state_->bind(&NativeBindingStateContext::StateChanged, this, false);
    }

    virtual ~NativeBindingStateContext()
    {
      const bool nodeAlive = !this->nodeDestroyed_ || !*this->nodeDestroyed_;
      const int value = this->state_->get();
      (void)value;
      this->state_->unbind(&NativeBindingStateContext::StateChanged, this);
      *this->unboundWhileStateAlive_ = nodeAlive;
    }

  private:
    static void StateChanged(void *)
    {
    }

    loka::core::State<int> *state_;
    bool *unboundWhileStateAlive_;
    const bool *nodeDestroyed_;
  };

  class NativeBindingStateBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<NativeBindingStateBoundaryNode>
  {
  public:
    explicit NativeBindingStateBoundaryNode(const NativeBindingStateBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<NativeBindingStateBoundaryNode>(props),
          nativeValue_(),
          initialized_(false)
    {
    }

    virtual ~NativeBindingStateBoundaryNode()
    {
      if (g_nativeBindingStateNodeDestroyed)
      {
        *g_nativeBindingStateNodeDestroyed = true;
      }
      if (g_nativeBindingStateBoundary == this)
      {
        g_nativeBindingStateBoundary = 0;
      }
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      if (this->initialized_)
      {
        return;
      }
      this->declareStates(1).state(this->nativeValue_, 42);
      g_nativeBindingStateBoundary = this;
      this->initialized_ = true;
    }

    virtual void detachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      if (g_nativeBindingStateBoundary == this)
      {
        g_nativeBindingStateBoundary = 0;
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      composition.declare(root);
    }

    loka::core::State<int> *nativeValueState() const
    {
      return this->nativeValue_.state();
    }

  private:
    loka::app::scene::NodeState<int> nativeValue_;
    bool initialized_;
  };

  class OwnedStateCorpseBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<OwnedStateCorpseBoundaryNode>
      OwnedStateCorpseBoundaryProps;

  int *g_ownedStateCorpseDestructorCalls = 0;
  OwnedStateCorpseBoundaryNode *g_ownedStateCorpseBoundary = 0;

  class OwnedStateCorpseBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<OwnedStateCorpseBoundaryNode>
  {
  public:
    explicit OwnedStateCorpseBoundaryNode(const OwnedStateCorpseBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<OwnedStateCorpseBoundaryNode>(props),
          ownedState_(),
          initialized_(false)
    {
    }

    virtual ~OwnedStateCorpseBoundaryNode()
    {
      if (g_ownedStateCorpseBoundary == this)
      {
        g_ownedStateCorpseBoundary = 0;
      }
      if (g_ownedStateCorpseDestructorCalls)
      {
        ++*g_ownedStateCorpseDestructorCalls;
      }
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      if (this->initialized_)
      {
        return;
      }
      composition.declareStates().state(this->ownedState_, 0);
      g_ownedStateCorpseBoundary = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      composition.declare(root);
    }

    bool ownsDeclaredState() const
    {
      return this->ownedState_.isValid() && this->ownedState_.dangerouslyOwner() == this;
    }

    void mutateOwnedStateDuringCorpse()
    {
      this->ownedState_.set(this->ownedState_.get() + 1);
      this->markViewDirty(loka::app::scene::NODE_DIRTY_PROPS);
    }

  private:
    loka::app::scene::NodeState<int> ownedState_;
    bool initialized_;
  };

  class ConditionalArenaRetireProbeNode;
  typedef loka::app::scene::BoundaryPropsFor<ConditionalArenaRetireProbeNode>
      ConditionalArenaRetireProbeProps;

  int *g_conditionalArenaRetireDestructorCalls = 0;
  int *g_conditionalArenaActiveDestructorCalls = 0;
  std::vector<int> *g_conditionalArenaRetireOrder = 0;
  std::vector<int> *g_conditionalArenaActiveOrder = 0;
  ConditionalArenaRetireProbeNode *g_conditionalArenaRetireProbe = 0;

  class ConditionalArenaRetireProbeNode
      : public loka::app::scene::BoundaryNodeFor<ConditionalArenaRetireProbeNode>
  {
  public:
    explicit ConditionalArenaRetireProbeNode(const ConditionalArenaRetireProbeProps &props)
        : loka::app::scene::BoundaryNodeFor<ConditionalArenaRetireProbeNode>(props),
          showAlternate_(),
          initialized_(false)
    {
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context,
                                    loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::BoundaryNodeFor<ConditionalArenaRetireProbeNode> BaseType;
      if (event != loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        BaseType::composeWithContext(context, event);
        return;
      }

      loka::app::scene::NodeComposition &composition = this->beginComposition(context);
      {
        loka::app::scene::NodeComposition::CompositionScope scope(composition);
        this->composeNode(composition);
      }
      this->captureCurrentCompositionSnapshot();
      this->rebuildCurrentCompositionDiff();
      std::vector<loka::app::scene::Node *> retainedChildren;
      if (!this->rebuildCompositionChildrenFromCurrentSnapshot(context, retainedChildren))
      {
        return;
      }
      this->promoteCurrentCompositionSnapshot();
      for (size_t i = 0; i < retainedChildren.size(); ++i)
      {
        if (retainedChildren[i])
        {
          this->composeSubtree(retainedChildren[i], context, event, this);
        }
      }
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      if (this->initialized_)
      {
        return;
      }
      composition.declareStates().state(this->showAlternate_, false);
      g_conditionalArenaRetireProbe = this;
      this->initialized_ = true;
    }

    virtual void detachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      if (g_conditionalArenaRetireProbe == this)
      {
        g_conditionalArenaRetireProbe = 0;
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      typedef loka::app::scene::NodeDefinition<ArenaRetireProbeProps, ArenaRetireProbeNode>
          ArenaRetireProbeDefinition;
      loka::app::Fragment root;
      const bool alternate = this->showAlternate_.get();
      if (g_conditionalArenaRetireOrder || g_conditionalArenaActiveOrder)
      {
        typedef loka::app::scene::NodeDefinition<ArenaRetireOrderLeafProps, ArenaRetireOrderLeafNode>
            ArenaRetireOrderLeafDefinition;
        std::vector<int> *order = alternate ? g_conditionalArenaActiveOrder : g_conditionalArenaRetireOrder;
        ArenaRetireOrderParentDefinition branch(
            (ArenaRetireOrderParentProps(order, alternate ? 4 : 2)));
        ArenaRetireOrderLeafDefinition child(
            (ArenaRetireOrderLeafProps(order, alternate ? 3 : 1)));
        branch << child;
        branch.tag(alternate ? 2 : 1);
        root << branch;
      }
      else if ((g_heapRetireParentDestructorCalls || g_heapRetireChildDestructorCalls) && !alternate)
      {
        ArenaParentWithHeapChildDefinition branch(
            (ArenaParentWithHeapChildProps(g_heapRetireParentDestructorCalls,
                                           g_heapRetireChildDestructorCalls)));
        branch.tag(1);
        root << branch;
      }
      else if (g_heapReplacementNestedBranch)
      {
        if (alternate)
        {
          NestedArenaRetireOuterDefinition branch(
              (NestedArenaRetireOuterProps(g_nestedArenaRetireOuterDestructorCalls)));
          loka::app::scene::BoundaryDefinition<NestedArenaRetireBoundaryProps,
                                               NestedArenaRetireBoundaryNode>
              nested = loka::app::scene::Boundary<NestedArenaRetireBoundaryNode>();
          branch << nested;
          branch.tag(2);
          root << branch;
        }
        else
        {
          ArenaRetireProbeDefinition branch(
              (ArenaRetireProbeProps(g_conditionalArenaRetireDestructorCalls)));
          branch.tag(1);
          root << branch;
        }
      }
      else if ((g_nestedArenaRetireOuterDestructorCalls ||
                g_nestedArenaRetireBoundaryDestructorCalls ||
                g_nestedArenaRetireLeafDestructorCalls) &&
               !alternate)
      {
        NestedArenaRetireOuterDefinition branch(
            (NestedArenaRetireOuterProps(g_nestedArenaRetireOuterDestructorCalls)));
        loka::app::scene::BoundaryDefinition<NestedArenaRetireBoundaryProps,
                                             NestedArenaRetireBoundaryNode>
            nested = loka::app::scene::Boundary<NestedArenaRetireBoundaryNode>();
        branch << nested;
        branch.tag(1);
        root << branch;
      }
      else if (g_includeNativeBindingRetireBranch && !alternate)
      {
        loka::app::scene::BoundaryDefinition<NativeBindingStateBoundaryProps,
                                             NativeBindingStateBoundaryNode>
            branch = loka::app::scene::Boundary<NativeBindingStateBoundaryNode>();
        branch.tag(1);
        root << branch;
      }
      else if (g_ownedStateCorpseDestructorCalls && !alternate)
      {
        loka::app::scene::BoundaryDefinition<OwnedStateCorpseBoundaryProps,
                                             OwnedStateCorpseBoundaryNode>
            branch = loka::app::scene::Boundary<OwnedStateCorpseBoundaryNode>();
        branch.tag(1);
        root << branch;
      }
      else
      {
        int *destructorCalls = alternate ? g_conditionalArenaActiveDestructorCalls
                                         : g_conditionalArenaRetireDestructorCalls;
        ArenaRetireProbeDefinition branch((ArenaRetireProbeProps(destructorCalls)));
        branch.tag(alternate ? 2 : 1);
        root << branch;
      }
      composition.declare(root);
    }

    loka::app::scene::Node *activeBranchNode() const
    {
      loka::app::scene::Node *root = this->compositionRootNode();
      loka::app::scene::INestable *nestable = root ? root->asNestable() : 0;
      return nestable ? nestable->childrenHead() : 0;
    }

    void showAlternate()
    {
      this->setAlternate(true);
    }

    void setAlternate(bool value)
    {
      this->showAlternate_.set(value, true);
      this->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
    }

  private:
    loka::app::scene::NodeState<bool> showAlternate_;
    bool initialized_;
  };

  class ObservedCorpseBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ObservedCorpseBoundaryNode> ObservedCorpseBoundaryProps;

  loka::core::MutableState<int> *g_observedCorpseExternalState = 0;
  int *g_observedCorpseDestructorCalls = 0;

  class ObservedCorpseBoundaryNode : public loka::app::scene::BoundaryNodeFor<ObservedCorpseBoundaryNode>
  {
  public:
    explicit ObservedCorpseBoundaryNode(const ObservedCorpseBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<ObservedCorpseBoundaryNode>(props)
    {
    }

    virtual ~ObservedCorpseBoundaryNode()
    {
      ++*g_observedCorpseDestructorCalls;
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(g_observedCorpseExternalState, loka::app::scene::NODE_DIRTY_PROPS);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      composition.declare(root);
    }
  };

  class CorpseRetireHarnessBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<CorpseRetireHarnessBoundaryNode> CorpseRetireHarnessBoundaryProps;

  CorpseRetireHarnessBoundaryNode *g_corpseRetireHarness = 0;

  class CorpseRetireHarnessBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<CorpseRetireHarnessBoundaryNode>
  {
  public:
    explicit CorpseRetireHarnessBoundaryNode(const CorpseRetireHarnessBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<CorpseRetireHarnessBoundaryNode>(props)
    {
      g_corpseRetireHarness = this;
    }

    virtual ~CorpseRetireHarnessBoundaryNode()
    {
      if (g_corpseRetireHarness == this)
      {
        g_corpseRetireHarness = 0;
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      loka::app::scene::BoundaryDefinition<ObservedCorpseBoundaryProps, ObservedCorpseBoundaryNode> branch =
          loka::app::scene::Boundary<ObservedCorpseBoundaryNode>();
      branch.tag(1);
      root << branch;
      composition.declare(root);
    }

    ObservedCorpseBoundaryNode *observedBranch() const
    {
      loka::app::scene::Node *root = this->compositionRootNode();
      loka::app::scene::INestable *nestable = root ? root->asNestable() : 0;
      loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
      return child ? static_cast<ObservedCorpseBoundaryNode *>(child->asBoundary()) : 0;
    }

    ObservedCorpseBoundaryNode *retireObservedBranch(loka::app::scene::Scene &scene,
                                                     loka::app::scene::IPlatformController &platform)
    {
      using namespace loka::app::scene;
      ObservedCorpseBoundaryNode *retired = this->observedBranch();
      assert(retired != 0);

      ComponentContext context;
      context.setBoundary(this);
      context.setStateOwner(this);
      context.setScene(&scene);
      context.setPlatformController(&platform);

      NodeDefinition<ArenaRetireProbeProps, ArenaRetireProbeNode> replacement(
          (ArenaRetireProbeProps(g_observedCorpseDestructorCalls)));
      replacement.tag(2);
      NodeComposition composition;
      composition.setContext(&context);
      Node *created = composition.createNodeFromDefinition(&replacement);
      assert(created != 0);

      BoundaryLocalRebuildPlan plan;
      plan.entries.push_back(BoundaryLocalRebuildPlanEntry::replace(created, retired, 2));
      std::vector<Node *> retainedChildren;
      INestable *root = this->compositionRootNestable();
      assert(root != 0);
      assert(this->applyLocalRebuildPlan(context, *root, plan, retainedChildren));
      return retired;
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

  struct NativeBindingProbePlatformController : public DetachProbePlatformController
  {
    explicit NativeBindingProbePlatformController(bool *unboundWhileStateAlive,
                                                  const bool *nodeDestroyed = 0)
        : unboundWhileStateAlive_(unboundWhileStateAlive),
          nodeDestroyed_(nodeDestroyed)
    {
    }

    virtual void onChange(loka::app::scene::Node *, loka::app::scene::NodeDirtyFlags, bool)
    {
      if (g_nativeBindingStateBoundary && !g_nativeBindingStateBoundary->getContext())
      {
        g_nativeBindingStateBoundary->setContext(
            new NativeBindingStateContext(g_nativeBindingStateBoundary->nativeValueState(),
                                          this->unboundWhileStateAlive_,
                                          this->nodeDestroyed_));
      }
    }

  private:
    bool *unboundWhileStateAlive_;
    const bool *nodeDestroyed_;
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

void testRootDetachChildWalkRetainsBoundaryStateOwner()
{
  using namespace loka::app::scene;

  DetachProbePlatformController platform;
  DetachStateOwnerObservation rootObservation;
  g_detachStateOwnerObservation = &rootObservation;
  {
    Scene scene((Boundary<RootDetachStateOwnerBoundaryNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    scene.unmount();
  }

  assert(rootObservation.detachCalls == 1);
  assert(rootObservation.currentBoundaryValid &&
         "root child detach must retain the root boundary as current state owner");
  assert(rootObservation.parentStateReadable &&
         "root child detach must read state declared by the root boundary");
  assert(rootObservation.parentStateValue == 37);

  DetachStateOwnerObservation nestedObservation;
  g_detachStateOwnerObservation = &nestedObservation;
  {
    Scene scene((Boundary<NestedDetachStateOwnerHarnessNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);
    scene.unmount();
  }

  assert(nestedObservation.detachCalls == 1);
  assert(nestedObservation.currentBoundaryValid);
  assert(nestedObservation.parentStateReadable);
  assert(nestedObservation.parentStateValue == 73);

  g_detachStateOwnerObservation = 0;
  g_detachParentState = 0;
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

void testConditionalBranchSwapNotifiesContextsAcrossRetainedDetach()
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
    assert(falseCounts.detachCalls == 1 &&
           "a retained detach must deliver the detach fact to the branch's contexts");

    condition.set(false);
    assert(falseCounts.attachCalls == 2 &&
           "re-entry must deliver the attach fact to the retained branch's contexts");
    assert(trueCounts.detachCalls == 1);
  }

  assert(trueCounts.attachCalls == 1);
  assert(falseCounts.attachCalls == 2);
  assert(trueCounts.detachCalls == 2 &&
         "context destruction must still fire the detach hook exactly once more");
  assert(falseCounts.detachCalls == 2);
}

void testConditionalSwapUnderHiddenAncestorStaysSilent()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  typedef NodeDefinition<PlainDetachProbeProps, PlainDetachProbeNode> PlainDetachProbeDefinition;

  DetachHookCounts outerFalseCounts;
  DetachHookCounts innerTrueCounts;
  DetachHookCounts innerFalseCounts;
  loka::core::MutableState<bool> outerCondition(true);
  loka::core::MutableState<bool> innerCondition(true);

  PlainDetachProbeDefinition innerTrueDef((PlainDetachProbeProps(&innerTrueCounts)));
  PlainDetachProbeDefinition innerFalseDef((PlainDetachProbeProps(&innerFalseCounts)));
  ConditionalDefinition innerDef((ConditionalProps(&innerCondition, &innerTrueDef, &innerFalseDef)));
  FragmentDefinition outerTrueWrap;
  outerTrueWrap << innerDef;
  PlainDetachProbeDefinition outerFalseDef((PlainDetachProbeProps(&outerFalseCounts)));

  {
    ConditionalNode outer((ConditionalProps(&outerCondition, &outerTrueWrap, &outerFalseDef)));
    assert(innerTrueCounts.attachCalls == 1);

    innerCondition.set(false);
    innerCondition.set(true);
    assert(innerTrueCounts.attachCalls == 2 && innerTrueCounts.detachCalls == 1);
    assert(innerFalseCounts.attachCalls == 1 && innerFalseCounts.detachCalls == 1);

    outerCondition.set(false);
    assert(innerTrueCounts.detachCalls == 2 &&
           "hiding the ancestor must deliver the detach fact down the active path");
    assert(innerFalseCounts.detachCalls == 1 &&
           "the inactive branch is already hidden and stays untouched");

    innerCondition.set(false);
    assert(innerFalseCounts.attachCalls == 1 &&
           "a swap under a hidden ancestor must not show the incoming branch");
    assert(innerTrueCounts.detachCalls == 2 &&
           "a swap under a hidden ancestor must stay silent");

    outerCondition.set(true);
    assert(innerFalseCounts.attachCalls == 2 &&
           "the ancestor's re-attach walk must show the then-active path");
    assert(innerTrueCounts.attachCalls == 2 &&
           "the branch swapped out while hidden must stay hidden");
  }

  assert(innerTrueCounts.detachCalls == 3);
  assert(innerFalseCounts.detachCalls == 2);
  assert(outerFalseCounts.detachCalls == 2);
}

void testConditionalBranchSwapNotifiesNestedContextsAcrossRetainedDetach()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  typedef NodeDefinition<PlainDetachProbeProps, PlainDetachProbeNode> PlainDetachProbeDefinition;

  DetachHookCounts innerCounts;
  DetachHookCounts falseCounts;
  loka::core::MutableState<bool> condition(true);
  FragmentDefinition trueWrapper;
  PlainDetachProbeDefinition innerDefinition((PlainDetachProbeProps(&innerCounts)));
  trueWrapper << innerDefinition;
  PlainDetachProbeDefinition falseDefinition((PlainDetachProbeProps(&falseCounts)));

  {
    ConditionalNode conditional((ConditionalProps(&condition, &trueWrapper, &falseDefinition)));
    assert(innerCounts.attachCalls == 1);
    assert(innerCounts.detachCalls == 0);

    condition.set(false);
    assert(innerCounts.detachCalls == 1 &&
           "a retained detach must reach contexts below the branch root");

    condition.set(true);
    assert(innerCounts.attachCalls == 2 &&
           "re-entry must reach contexts below the branch root");
  }

  assert(innerCounts.attachCalls == 2);
  assert(innerCounts.detachCalls == 2);
}

void testConditionalBranchSwapDestroysRetiredArenaNodeOnNextTrackerRun()
{
  using namespace loka::app::scene;

  int destructorCalls = 0;
  g_conditionalArenaRetireDestructorCalls = &destructorCalls;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));

    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    Node *retiringBranch = g_conditionalArenaRetireProbe->activeBranchNode();
    assert(retiringBranch != 0);
    assert(retiringBranch->isArenaAllocated() &&
           "conditional retire test must exercise an arena-allocated branch");
    assert(destructorCalls == 0);

    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());

    assert(g_conditionalArenaRetireProbe->activeBranchNode() != retiringBranch &&
           "conditional tag swap must replace the retiring branch");
    assert(scene.hasPendingInvalidation() &&
           "arena retirement must schedule a later tracker run");
    assert(destructorCalls == 0 &&
           "retired arena node must remain alive through the retiring apply");

    assert(!scene.flushInvalidation() &&
           "a drain-only tracker run must not report a refresh snapshot");

    assert(destructorCalls == 1 &&
           "retired arena node destructor must run at the next tracker run");
  }

  assert(destructorCalls == 1 &&
         "Scene teardown must not destroy an already drained arena node again");

  g_conditionalArenaRetireDestructorCalls = 0;
  g_conditionalArenaActiveDestructorCalls = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testRootUpdateFallbackDestroysRetiredArenaNodeOnNextTrackerRun()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  int destructorCalls = 0;
  bool useAlternate = false;
  NodeDefinition<ArenaRetireProbeProps, ArenaRetireProbeNode> retiring(
      (ArenaRetireProbeProps(&destructorCalls)));
  Fragment alternate;

  {
    DetachProbePlatformController platform;
    RootUpdateFallbackTestScene scene(
        new RootUpdateFallbackDefinition(&useAlternate, &retiring, &alternate));

    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = scene.rootBoundary();
    assert(rootBoundary != 0);
    Node *retiringRoot = rootBoundary->childrenHead();
    assert(retiringRoot != 0);
    assert(retiringRoot->isArenaAllocated() &&
           "root fallback retire test must exercise an arena-allocated root");
    assert(destructorCalls == 0);

    useAlternate = true;
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());

    assert(rootBoundary->childrenHead() != retiringRoot &&
           "root shape swap must replace the retiring root");
    assert(destructorCalls == 0 &&
           "retired root arena node must remain alive through the retiring apply");
    assert(scene.hasPendingInvalidation() &&
           "root arena retirement must schedule a later tracker run");

    assert(!scene.flushInvalidation() &&
           "a drain-only tracker run must not report a refresh snapshot");
    assert(destructorCalls == 1 &&
           "retired root arena node destructor must run at the next tracker run");
  }

  assert(destructorCalls == 1 &&
         "Scene teardown must not destroy an already drained root arena node again");
}

void testRootUpdateFallbackReservesFreshArenaGeneration()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  int destructorCalls = 0;
  std::vector<int> alternateDestroyOrder;
  bool useAlternate = false;
  NodeDefinition<ArenaRetireProbeProps, ArenaRetireProbeNode> retiring(
      (ArenaRetireProbeProps(&destructorCalls)));
  ArenaRetireOrderParentDefinition alternate(
      (ArenaRetireOrderParentProps(&alternateDestroyOrder, 2)));

  {
    DetachProbePlatformController platform;
    RootUpdateFallbackTestScene scene(
        new RootUpdateFallbackDefinition(&useAlternate, &retiring, &alternate));

    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = scene.rootBoundary();
    assert(rootBoundary != 0);
    Node *initialRoot = rootBoundary->childrenHead();
    assert(initialRoot != 0 && initialRoot->isArenaAllocated());

    useAlternate = true;
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());

    Node *alternateRoot = rootBoundary->childrenHead();
    assert(alternateRoot != 0 && alternateRoot != initialRoot);
    assert(alternateRoot->isArenaAllocated() &&
           "root fallback must reserve a fresh arena generation");
    assert(destructorCalls == 0);
    assert(alternateDestroyOrder.empty());

    assert(!scene.flushInvalidation());
    assert(destructorCalls == 1 &&
           "the first retired generation must be reclaimed exactly once at the next run");
    assert(alternateDestroyOrder.empty());

    useAlternate = false;
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());

    Node *secondRoot = rootBoundary->childrenHead();
    assert(secondRoot != 0 && secondRoot != alternateRoot);
    assert(secondRoot->isArenaAllocated() &&
           "every root fallback must reserve a fresh arena generation");
    assert(destructorCalls == 1);
    assert(alternateDestroyOrder.empty() &&
           "the second retired generation must remain alive through the retiring apply");

    assert(!scene.flushInvalidation());
    assert(destructorCalls == 1 &&
           "draining the second retired generation must not revisit the first");
    assert(alternateDestroyOrder.size() == 1 && alternateDestroyOrder[0] == 2 &&
           "the second retired generation must be reclaimed exactly once at the next run");
  }

  assert(destructorCalls == 2 &&
         "Scene teardown must destroy the active generation exactly once");
  assert(alternateDestroyOrder.size() == 1);
}

void testRetiredGenerationSubsumesQueuedArenaSubtreeExactlyOnce()
{
  using namespace loka::app::scene;

  int queuedDestructorCalls = 0;
  int ledgerDestructorCalls = 0;

  {
    RetiredGenerationOverlapBoundary boundary;
    boundary.reserveProbeNodes(2);
    Node *queued = boundary.createProbeNode(&queuedDestructorCalls);
    Node *ledgerOnly = boundary.createProbeNode(&ledgerDestructorCalls);
    assert(queued->isArenaAllocated());
    assert(ledgerOnly->isArenaAllocated());

    boundary.queueSubtreeThenRetireGeneration(queued);

    assert(!boundary.nodeArena()->hasCapacity() &&
           "generation retirement must detach the arena after queue subsumption");
    assert(queuedDestructorCalls == 0);
    assert(ledgerDestructorCalls == 0);

    boundary.drainRetiredSubtreesAtNextTrackerRun();
    assert(queuedDestructorCalls == 1 &&
           "the subtree-queued node must be destroyed once by its retired generation");
    assert(ledgerDestructorCalls == 1 &&
           "the remaining generation ledger node must be destroyed once");

    boundary.drainRetiredSubtreesAtNextTrackerRun();
    assert(queuedDestructorCalls == 1);
    assert(ledgerDestructorCalls == 1);
  }

  assert(queuedDestructorCalls == 1 &&
         "Boundary teardown must not revisit the subsumed queue entry");
  assert(ledgerDestructorCalls == 1 &&
         "Boundary teardown must not revisit the drained generation");
}

void testRootUpdateFallbackReleasesNativeContextBeforeNodeOwnedStateReclaim()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  bool unboundWhileStateAlive = false;
  bool nodeDestroyed = false;
  bool useAlternate = false;
  BoundaryDefinition<NativeBindingStateBoundaryProps, NativeBindingStateBoundaryNode> retiring =
      Boundary<NativeBindingStateBoundaryNode>();
  Fragment alternate;
  g_nativeBindingStateNodeDestroyed = &nodeDestroyed;

  {
    NativeBindingProbePlatformController platform(&unboundWhileStateAlive, &nodeDestroyed);
    RootUpdateFallbackTestScene scene(
        new RootUpdateFallbackDefinition(&useAlternate, &retiring, &alternate));

    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_nativeBindingStateBoundary != 0);
    assert(g_nativeBindingStateBoundary->getContext() != 0);
    assert(!unboundWhileStateAlive);
    assert(!nodeDestroyed);

    useAlternate = true;
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());

    assert(unboundWhileStateAlive &&
           "root fallback must release native context before node-owned state reclaim");
    assert(!nodeDestroyed &&
           "root fallback arena node must remain alive through the retiring apply");

    assert(!scene.flushInvalidation() &&
           "native-context retire drain must not report a refresh snapshot");
    assert(nodeDestroyed &&
           "root fallback arena node must be reclaimed at the next tracker run");
  }

  g_nativeBindingStateNodeDestroyed = 0;
  g_nativeBindingStateBoundary = 0;
}

void testRootReplacementDestroysRetiredArenaNodeOnNextTrackerRun()
{
  using namespace loka::app::scene;

  int retiredDestructorCalls = 0;
  g_rootReplacementRetireDestructorCalls = &retiredDestructorCalls;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<RootReplacementArenaRetireBoundaryNode>()));

    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_rootReplacementArenaRetireBoundary != 0);
    Node *retiringRoot = g_rootReplacementArenaRetireBoundary->activeRootNode();
    assert(retiringRoot != 0);
    assert(retiringRoot->asNestable() == 0 &&
           "root replacement retire test must exercise a non-nestable root");
    assert(retiringRoot->isArenaAllocated() &&
           "root replacement retire test must exercise an arena-allocated root");
    assert(retiredDestructorCalls == 0);

    g_rootReplacementArenaRetireBoundary->showReplacement();
    assert(scene.flushInvalidation());

    assert(g_rootReplacementArenaRetireBoundary->activeRootNode() != retiringRoot &&
           "incompatible root definition type must replace the retiring root");
    assert(retiredDestructorCalls == 0 &&
           "retired arena root must remain alive through the retiring apply");

    assert(!scene.flushInvalidation() &&
           "a drain-only tracker run must not report a refresh snapshot");

    assert(retiredDestructorCalls == 1 &&
           "retired arena root destructor must run at the next tracker run");
  }

  assert(retiredDestructorCalls == 1 &&
         "Scene teardown must not destroy an already drained arena root again");

  g_rootReplacementRetireDestructorCalls = 0;
  g_rootReplacementArenaRetireBoundary = 0;
}


void testRetiringNativeContextUnbindsBeforeNodeOwnedStateReclaim()
{
  using namespace loka::app::scene;

  bool unboundWhileStateAlive = false;
  g_includeNativeBindingRetireBranch = true;

  {
    NativeBindingProbePlatformController platform(&unboundWhileStateAlive);
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));

    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    assert(g_nativeBindingStateBoundary != 0);
    assert(g_nativeBindingStateBoundary->getContext() != 0);

    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());
    assert(unboundWhileStateAlive &&
           "retiring native context must unbind before its node-owned state is reclaimed");
  }

  g_includeNativeBindingRetireBranch = false;
  g_nativeBindingStateBoundary = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testSceneDestructionUnbindsNativeContextBeforeNodeOwnedStateReclaim()
{
  using namespace loka::app::scene;

  bool unboundWhileStateAlive = false;
  NativeBindingProbePlatformController platform(&unboundWhileStateAlive);

  {
    Scene scene((Boundary<NativeBindingStateBoundaryNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_nativeBindingStateBoundary != 0);
    assert(g_nativeBindingStateBoundary->getContext() != 0);
    assert(!unboundWhileStateAlive);
  }

  assert(unboundWhileStateAlive &&
         "Scene destruction must unbind native context before its node-owned state is reclaimed");
  g_nativeBindingStateBoundary = 0;
}

void testSceneTeardownDrainsNonEmptyRetiredArenaSubtreeExactlyOnce()
{
  using namespace loka::app::scene;

  std::vector<int> destroyOrder;
  g_conditionalArenaRetireOrder = &destroyOrder;
  g_conditionalArenaActiveOrder = &destroyOrder;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    Node *retiringBranch = g_conditionalArenaRetireProbe->activeBranchNode();
    assert(retiringBranch != 0 && retiringBranch->isArenaAllocated());

    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());
    assert(scene.hasPendingInvalidation() &&
           "retirement must leave a later drain pending at Scene teardown");
    assert(destroyOrder.empty() &&
           "the retiring flush must not reclaim the queued arena subtree");
  }

  assert(destroyOrder.size() == 4);
  assert(destroyOrder[0] == 1 && destroyOrder[1] == 2 &&
         "Scene teardown must drain the retired subtree before clearing live arena storage");
  assert(destroyOrder[2] == 4 && destroyOrder[3] == 3);

  g_conditionalArenaRetireOrder = 0;
  g_conditionalArenaActiveOrder = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testSceneTeardownDrainsPendingRetiredGenerationExactlyOnce()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  int destructorCalls = 0;
  bool useAlternate = false;
  NodeDefinition<ArenaRetireProbeProps, ArenaRetireProbeNode> retiring(
      (ArenaRetireProbeProps(&destructorCalls)));
  Fragment alternate;

  {
    DetachProbePlatformController platform;
    RootUpdateFallbackTestScene scene(
        new RootUpdateFallbackDefinition(&useAlternate, &retiring, &alternate));
    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = scene.rootBoundary();
    assert(rootBoundary != 0);
    Node *retiringRoot = rootBoundary->childrenHead();
    assert(retiringRoot != 0 && retiringRoot->isArenaAllocated());

    useAlternate = true;
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    assert(scene.flushInvalidation());

    assert(rootBoundary->childrenHead() != retiringRoot);
    assert(rootBoundary->childrenHead() != 0 &&
           rootBoundary->childrenHead()->isArenaAllocated());
    assert(scene.hasPendingInvalidation() &&
           "generation retirement must leave a later drain pending at Scene teardown");
    assert(destructorCalls == 0 &&
           "the retiring flush must not reclaim the pending generation");
  }

  assert(destructorCalls == 1 &&
         "Scene teardown must drain a pending retired generation exactly once");
}

void testConditionalBranchSwapDestroysRetiredArenaSubtreeChildrenFirst()
{
  using namespace loka::app::scene;

  std::vector<int> destroyOrder;
  g_conditionalArenaRetireOrder = &destroyOrder;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    Node *retiringBranch = g_conditionalArenaRetireProbe->activeBranchNode();
    assert(retiringBranch != 0 && retiringBranch->isArenaAllocated());

    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());
    assert(scene.hasPendingInvalidation() &&
           "arena subtree retirement must schedule a later tracker run");
    assert(destroyOrder.empty() &&
           "retired arena subtree must remain alive through the retiring apply");

    assert(!scene.flushInvalidation() &&
           "a drain-only tracker run must not report a refresh snapshot");
    assert(destroyOrder.size() == 2);
    assert(destroyOrder[0] == 1 &&
           "retired arena subtree must destroy children before parents");
    assert(destroyOrder[1] == 2);
  }

  assert(destroyOrder.size() == 2 &&
         "Scene teardown must not append duplicate subtree destructor records");
  assert(destroyOrder[0] == 1);
  assert(destroyOrder[1] == 2);

  g_conditionalArenaRetireOrder = 0;
  g_conditionalArenaActiveOrder = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testRetiredArenaParentDestroysHeapChildExactlyOnceAtDrain()
{
  using namespace loka::app::scene;

  int parentDestructorCalls = 0;
  int heapChildDestructorCalls = 0;
  g_heapRetireParentDestructorCalls = &parentDestructorCalls;
  g_heapRetireChildDestructorCalls = &heapChildDestructorCalls;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    Node *retiringBranch = g_conditionalArenaRetireProbe->activeBranchNode();
    INestable *retiringParent = retiringBranch ? retiringBranch->asNestable() : 0;
    Node *heapChild = retiringParent ? retiringParent->childrenHead() : 0;
    assert(retiringBranch != 0 && retiringBranch->isArenaAllocated());
    assert(heapChild != 0 && !heapChild->isArenaAllocated() &&
           "drain test must place a heap child under the retired arena parent");

    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());
    assert(scene.hasPendingInvalidation());
    assert(parentDestructorCalls == 0);
    assert(heapChildDestructorCalls == 0);

    assert(!scene.flushInvalidation() &&
           "heap-child reclamation alone must not report a refresh snapshot");
    assert(parentDestructorCalls == 1);
    assert(heapChildDestructorCalls == 1);
  }

  assert(parentDestructorCalls == 1);
  assert(heapChildDestructorCalls == 1 &&
         "Scene teardown must not delete an already drained heap child again");

  g_heapRetireParentDestructorCalls = 0;
  g_heapRetireChildDestructorCalls = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testConditionalBranchSwapBackDestroysRetiredHeapNodeOnNextTrackerRun()
{
  using namespace loka::app::scene;

  int retireCalls = 0;
  int activeCalls = 0;
  g_conditionalArenaRetireDestructorCalls = &retireCalls;
  g_conditionalArenaActiveDestructorCalls = &activeCalls;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    Node *initialBranch = g_conditionalArenaRetireProbe->activeBranchNode();
    assert(initialBranch != 0 && initialBranch->isArenaAllocated());

    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());
    Node *heapBranch = g_conditionalArenaRetireProbe->activeBranchNode();
    assert(heapBranch != 0 && heapBranch != initialBranch);
    assert(!heapBranch->isArenaAllocated() &&
           "the swapped-in branch must land on the heap once the slab is consumed");
    assert(!scene.flushInvalidation());
    assert(retireCalls == 1 && activeCalls == 0);

    g_conditionalArenaRetireProbe->setAlternate(false);
    assert(scene.flushInvalidation());
    assert(activeCalls == 0 &&
           "the retired heap branch must remain alive through the retiring apply");

    assert(!scene.flushInvalidation() &&
           "a drain-only tracker run must not report a refresh snapshot");
    assert(activeCalls == 1 &&
           "the retired heap branch destructor must run at the next tracker run");
    assert(retireCalls == 1);
  }

  assert(activeCalls == 1 &&
         "Scene teardown must not delete an already drained heap branch again");
  assert(retireCalls == 2 &&
         "Scene teardown must reclaim the swapped-back heap branch exactly once");

  g_conditionalArenaRetireDestructorCalls = 0;
  g_conditionalArenaActiveDestructorCalls = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testPendingChildBoundaryUpdateSurvivesHeapSubtreeReplacement()
{
  using namespace loka::app::scene;

  int outerDestructorCalls = 0;
  int nestedBoundaryDestructorCalls = 0;
  g_heapReplacementNestedBranch = true;
  g_nestedArenaRetireOuterDestructorCalls = &outerDestructorCalls;
  g_nestedArenaRetireBoundaryDestructorCalls = &nestedBoundaryDestructorCalls;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());
    assert(!scene.flushInvalidation());

    Node *outer = g_conditionalArenaRetireProbe->activeBranchNode();
    assert(outer != 0 && !outer->isArenaAllocated() &&
           "the replacement subtree must land on the heap once the slab is consumed");
    INestable *outerNestable = outer->asNestable();
    Node *nestedNode = outerNestable ? outerNestable->childrenHead() : 0;
    BoundaryNode *nested = nestedNode ? nestedNode->asBoundary() : 0;
    assert(nested != 0 && !nestedNode->isArenaAllocated());

    nested->markViewDirty(NODE_DIRTY_PROPS);
    assert(scene.director().hasPendingBoundary(nested) &&
           "the nested boundary must be a pending update target before the swap");

    g_conditionalArenaRetireProbe->setAlternate(false);
    assert(scene.flushInvalidation() &&
           "the flush that replaces the pending target's subtree must complete");
    assert(nestedBoundaryDestructorCalls == 0 && outerDestructorCalls == 0 &&
           "the replaced heap subtree must remain alive through the flush that walks its pending entry");

    assert(!scene.flushInvalidation());
    assert(nestedBoundaryDestructorCalls == 1 && outerDestructorCalls == 1 &&
           "the replaced heap subtree must be reclaimed at the next tracker run");
  }

  assert(nestedBoundaryDestructorCalls == 1);
  assert(outerDestructorCalls == 1);

  g_heapReplacementNestedBranch = false;
  g_nestedArenaRetireOuterDestructorCalls = 0;
  g_nestedArenaRetireBoundaryDestructorCalls = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testRetiredHeapParentDestroysArenaChildExactlyOnceAtDrain()
{
  using namespace loka::app::scene;

  int heapParentCalls = 0;
  int heapChildCalls = 0;
  int arenaChildCalls = 0;

  {
    RetiredGenerationOverlapBoundary boundary;
    boundary.reserveProbeNodes(1);
    Node *arenaChild = boundary.createProbeNode(&arenaChildCalls);
    ArenaParentWithHeapChildNode *heapRoot = new ArenaParentWithHeapChildNode(
        (ArenaParentWithHeapChildProps(&heapParentCalls, &heapChildCalls)));
    heapRoot->addChild(arenaChild);
    assert(!heapRoot->isArenaAllocated());
    assert(arenaChild->isArenaAllocated());

    boundary.queueSubtree(heapRoot);
    assert(heapParentCalls == 0 && heapChildCalls == 0 && arenaChildCalls == 0 &&
           "the queued heap subtree must remain alive through the retiring apply");

    boundary.drainRetiredSubtreesAtNextTrackerRun();
    assert(heapParentCalls == 1 && heapChildCalls == 1 && arenaChildCalls == 1 &&
           "the drain must reclaim the heap root, its heap child, and its arena child exactly once");

    boundary.drainRetiredSubtreesAtNextTrackerRun();
    assert(heapParentCalls == 1 && heapChildCalls == 1 && arenaChildCalls == 1);
  }

  assert(heapParentCalls == 1 && heapChildCalls == 1);
  assert(arenaChildCalls == 1 &&
         "Boundary teardown must not revisit the tombstoned arena child");
}

void testRetiredGenerationSubsumesQueuedHeapSubtreeExactlyOnce()
{
  using namespace loka::app::scene;

  int heapParentCalls = 0;
  int heapChildCalls = 0;
  int arenaChildCalls = 0;

  {
    RetiredGenerationOverlapBoundary boundary;
    boundary.reserveProbeNodes(1);
    Node *arenaChild = boundary.createProbeNode(&arenaChildCalls);
    ArenaParentWithHeapChildNode *heapRoot = new ArenaParentWithHeapChildNode(
        (ArenaParentWithHeapChildProps(&heapParentCalls, &heapChildCalls)));
    heapRoot->addChild(arenaChild);
    assert(!heapRoot->isArenaAllocated());

    boundary.queueSubtreeThenRetireGeneration(heapRoot);
    assert(!boundary.nodeArena()->hasCapacity() &&
           "generation retirement must detach the arena after subsuming the queue");
    assert(heapParentCalls == 0 && heapChildCalls == 0 && arenaChildCalls == 0);

    boundary.drainRetiredSubtreesAtNextTrackerRun();
    assert(heapParentCalls == 1 &&
           "the subsumed heap root must be destroyed once by its retired generation");
    assert(heapChildCalls == 1 && arenaChildCalls == 1);

    boundary.drainRetiredSubtreesAtNextTrackerRun();
    assert(heapParentCalls == 1 && heapChildCalls == 1 && arenaChildCalls == 1);
  }

  assert(heapParentCalls == 1 && heapChildCalls == 1);
  assert(arenaChildCalls == 1 &&
         "Boundary teardown must not revisit the drained generation");
}

void testRetiredSubtreeDestroysNestedBoundaryArenaExactlyOnce()
{
  using namespace loka::app::scene;

  int outerDestructorCalls = 0;
  int nestedBoundaryDestructorCalls = 0;
  int innerArenaChildDestructorCalls = 0;
  g_nestedArenaRetireOuterDestructorCalls = &outerDestructorCalls;
  g_nestedArenaRetireBoundaryDestructorCalls = &nestedBoundaryDestructorCalls;
  g_nestedArenaRetireLeafDestructorCalls = &innerArenaChildDestructorCalls;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    Node *retiringBranch = g_conditionalArenaRetireProbe->activeBranchNode();
    INestable *outer = retiringBranch ? retiringBranch->asNestable() : 0;
    Node *nestedNode = outer ? outer->childrenHead() : 0;
    NestedArenaRetireBoundaryNode *nested =
        nestedNode ? static_cast<NestedArenaRetireBoundaryNode *>(nestedNode->asBoundary()) : 0;
    Node *innerArenaChild = nested ? nested->innerArenaChild() : 0;
    assert(retiringBranch != 0 && retiringBranch->isArenaAllocated());
    assert(nested != 0 && nested->isArenaAllocated());
    assert(innerArenaChild != 0 && innerArenaChild->isArenaAllocated() &&
           "nested boundary test must exercise the nested Boundary's own node arena");

    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());
    assert(scene.hasPendingInvalidation());
    assert(outerDestructorCalls == 0);
    assert(nestedBoundaryDestructorCalls == 0);
    assert(innerArenaChildDestructorCalls == 0);

    assert(!scene.flushInvalidation() &&
           "nested arena reclamation alone must not report a refresh snapshot");
    assert(outerDestructorCalls == 1);
    assert(nestedBoundaryDestructorCalls == 1);
    assert(innerArenaChildDestructorCalls == 1);
  }

  assert(outerDestructorCalls == 1);
  assert(nestedBoundaryDestructorCalls == 1);
  assert(innerArenaChildDestructorCalls == 1);

  g_nestedArenaRetireOuterDestructorCalls = 0;
  g_nestedArenaRetireBoundaryDestructorCalls = 0;
  g_nestedArenaRetireLeafDestructorCalls = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testRetiredBoundaryOwnedStateMutationIsQuiescent()
{
  using namespace loka::app::scene;

  int destructorCalls = 0;
  g_ownedStateCorpseDestructorCalls = &destructorCalls;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<ConditionalArenaRetireProbeNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalArenaRetireProbe != 0);
    OwnedStateCorpseBoundaryNode *retired = g_ownedStateCorpseBoundary;
    assert(retired != 0);
    assert(retired == g_conditionalArenaRetireProbe->activeBranchNode()->asBoundary());
    assert(retired->isArenaAllocated());
    assert(retired->ownsDeclaredState() &&
           "corpse quiescence test requires state owned by the retired Boundary");

    g_conditionalArenaRetireProbe->showAlternate();
    assert(scene.flushInvalidation());
    assert(scene.hasPendingInvalidation());
    assert(destructorCalls == 0);

    retired->mutateOwnedStateDuringCorpse();
    assert(!scene.flushInvalidation() &&
           "owned state mutation on a detached Boundary must not produce a refresh snapshot");
    assert(destructorCalls == 1);
  }

  assert(destructorCalls == 1);

  g_ownedStateCorpseDestructorCalls = 0;
  g_ownedStateCorpseBoundary = 0;
  g_conditionalArenaRetireProbe = 0;
}

void testRetiredBoundaryIsQuiescentBeforeNextTrackerRun()
{
  using namespace loka::app::scene;

  int destructorCalls = 0;
  loka::core::MutableState<int> externalState(0);
  g_observedCorpseExternalState = &externalState;
  g_observedCorpseDestructorCalls = &destructorCalls;

  {
    DetachProbePlatformController platform;
    Scene scene((Boundary<CorpseRetireHarnessBoundaryNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_corpseRetireHarness != 0);
    ObservedCorpseBoundaryNode *retired = g_corpseRetireHarness->observedBranch();
    assert(retired != 0);
    assert(retired->isArenaAllocated());
    assert(g_corpseRetireHarness->retireObservedBranch(scene, platform) == retired);
    assert(destructorCalls == 0);

    externalState.set(1);
    assert(!scene.director().hasPendingBoundary(retired) &&
           "detached observed boundary must not enqueue projection work while awaiting reclaim");

    assert(!scene.flushInvalidation() &&
           "quiescent corpse reclamation alone must not report a refresh snapshot");
  }

  g_observedCorpseExternalState = 0;
  g_observedCorpseDestructorCalls = 0;
  g_corpseRetireHarness = 0;
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
    assert(falseCounts.detachCalls == 1 &&
           "a retained detach must deliver the detach fact to the branch's contexts");

    scene.unmount();

    // Teardown releases each branch context exactly once more.
    assert(trueCounts.detachCalls == 1);
    assert(falseCounts.detachCalls == 2);
    assert(trueCounts.attachCalls == 1);
    assert(falseCounts.attachCalls == 1);
  }

  g_conditionalSceneTrueCounts = 0;
  g_conditionalSceneFalseCounts = 0;
  g_conditionalSceneCondition = 0;
}
