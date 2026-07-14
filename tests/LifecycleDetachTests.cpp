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
      this->showAlternate_.set(true, true);
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
