#include "AttachNodeContractTests.hpp"
#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>

#include "app/nodes/boundary/RecomposingBoundary.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "core/State.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/RecomposingBoundary.hpp"

namespace
{
  struct AttachReplayScenario
  {
    AttachReplayScenario()
        : visible(true),
          guardedEmitter(),
          unguardedEmitter(),
          guardedConstructions(0),
          guardedAttaches(0),
          guardedCalls(0),
          unguardedCalls(0)
    {
    }

    loka::core::MutableState<bool> visible;
    loka::core::EmitterState guardedEmitter;
    loka::core::EmitterState unguardedEmitter;
    int guardedConstructions;
    int guardedAttaches;
    int guardedCalls;
    int unguardedCalls;
  };

  AttachReplayScenario *g_attachReplayScenario = 0;

  template <class NodeT> struct AttachComponentTypeTag
  {
  };

  template <class NodeT>
  struct AttachComponentProps
      : public loka::app::scene::NodePropsBase<AttachComponentProps<NodeT> >
  {
    typedef AttachComponentTypeTag<NodeT> TypeTag;
    typedef NodeT NodeType;

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      return false;
    }
  };

  class GuardedAttachComponentNode;
  typedef AttachComponentProps<GuardedAttachComponentNode>
      GuardedAttachComponentProps;

  class GuardedAttachComponentNode
      : public loka::app::scene::ComponentNodeWithProps<GuardedAttachComponentProps>
  {
  public:
    explicit GuardedAttachComponentNode(const GuardedAttachComponentProps &props)
        : loka::app::scene::ComponentNodeWithProps<GuardedAttachComponentProps>(props),
          initialized_(false)
    {
      ++g_attachReplayScenario->guardedConstructions;
    }

    virtual void declareBindings(loka::app::scene::BindingToken &t)
    {
      t.action(g_attachReplayScenario->guardedEmitter, this, &GuardedAttachComponentNode::recordCall);
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      ++g_attachReplayScenario->guardedAttaches;
      if (this->initialized_)
      {
        return;
      }
      this->initialized_ = true;
    }

    virtual void composeChildren(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::Button(
          "guarded", &g_attachReplayScenario->guardedEmitter));
    }

  private:
    void recordCall()
    {
      ++g_attachReplayScenario->guardedCalls;
    }

    bool initialized_;
  };

  class UnguardedAttachComponentNode;
  typedef AttachComponentProps<UnguardedAttachComponentNode>
      UnguardedAttachComponentProps;

  class UnguardedAttachComponentNode
      : public loka::app::scene::ComponentNodeWithProps<UnguardedAttachComponentProps>
  {
  public:
    explicit UnguardedAttachComponentNode(const UnguardedAttachComponentProps &props)
        : loka::app::scene::ComponentNodeWithProps<UnguardedAttachComponentProps>(props)
    {
    }

    virtual void declareBindings(loka::app::scene::BindingToken &t)
    {
      t.action(g_attachReplayScenario->unguardedEmitter, this, &UnguardedAttachComponentNode::recordCall);
    }

    virtual void composeChildren(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::Button(
          "unguarded", &g_attachReplayScenario->unguardedEmitter));
    }

  private:
    void recordCall()
    {
      ++g_attachReplayScenario->unguardedCalls;
    }
  };

  class AttachReplayRootNode;
  typedef loka::app::scene::BoundaryPropsFor<AttachReplayRootNode>
      AttachReplayRootProps;

  class AttachReplayRootNode
      : public loka::app::scene::BoundaryNodeFor<AttachReplayRootNode>
  {
  public:
    explicit AttachReplayRootNode(const AttachReplayRootProps &props)
        : loka::app::scene::BoundaryNodeFor<AttachReplayRootNode>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(
        loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(
        loka::app::scene::DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(&g_attachReplayScenario->visible,
                                  loka::app::scene::NODE_DIRTY_CHILD);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::ShowDefinition shown =
          loka::app::Show(g_attachReplayScenario->visible);
      shown << loka::app::scene::Component(GuardedAttachComponentProps())
            << loka::app::scene::Component(UnguardedAttachComponentProps());
      shown.setNodeTag(5361);
      loka::app::Fragment root;
      root << shown;
      composition.declare(root);
    }
  };

  template <class NodeT> struct PointerBindTypeTag
  {
  };

  class PropsPointerBindBoundaryNode;
  class DefinitionPointerBindBoundaryNode;

  template <class NodeT>
  struct PointerBindProps
      : public loka::app::scene::NodePropsBase<PointerBindProps<NodeT> >
  {
    typedef PointerBindTypeTag<NodeT> TypeTag;
    typedef NodeT NodeType;

    PointerBindProps()
        : emitter(0), calls(0)
    {
    }

    PointerBindProps(loka::core::EmitterState *source, int *callCount)
        : emitter(source), calls(callCount)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return false;
      }
      const PointerBindProps<NodeT> &other =
          static_cast<const PointerBindProps<NodeT> &>(rhs);
      if (this->emitter != other.emitter)
      {
        return this->emitter < other.emitter;
      }
      return this->calls < other.calls;
    }

    loka::core::EmitterState *emitter;
    int *calls;
  };

  typedef PointerBindProps<PropsPointerBindBoundaryNode>
      PropsPointerBindProps;
  typedef PointerBindProps<DefinitionPointerBindBoundaryNode>
      DefinitionPointerBindProps;

  class PropsPointerBindBoundaryNode
      : public loka::app::scene::RecomposingBoundaryFor<PropsPointerBindBoundaryNode,
            loka::app::scene::StdCompositionBoundaryNodeBase<PropsPointerBindProps> >
  {
  public:
    explicit PropsPointerBindBoundaryNode(const PropsPointerBindProps &props)
        : loka::app::scene::RecomposingBoundaryFor<PropsPointerBindBoundaryNode,
              loka::app::scene::StdCompositionBoundaryNodeBase<PropsPointerBindProps> >(props)
    {
    }

    virtual void declareBindings(loka::app::scene::BindingToken &t)
    {
      t.action(*this->props.emitter, this, &PropsPointerBindBoundaryNode::recordCall);
    }

  private:
    void recordCall()
    {
      ++*this->props.calls;
    }
  };

  template <class NodeT, class PropsT>
  class PropsRecomposingBoundaryNode
      : public loka::app::scene::RecomposingBoundaryFor<
            NodeT, loka::app::scene::StdCompositionBoundaryNodeBase<PropsT> >
  {
  public:
    explicit PropsRecomposingBoundaryNode(const PropsT &props)
        : loka::app::scene::RecomposingBoundaryFor<
              NodeT, loka::app::scene::StdCompositionBoundaryNodeBase<PropsT> >(props)
    {
    }
  };

  class DefinitionPointerBindBoundaryNode
      : public PropsRecomposingBoundaryNode<
            DefinitionPointerBindBoundaryNode, DefinitionPointerBindProps>
  {
  public:
    explicit DefinitionPointerBindBoundaryNode(
        const DefinitionPointerBindProps &props)
        : PropsRecomposingBoundaryNode<DefinitionPointerBindBoundaryNode,
                                       DefinitionPointerBindProps>(props)
    {
    }

    virtual void declareBindings(loka::app::scene::BindingToken &t)
    {
      t.action(*this->props.emitter, this, &DefinitionPointerBindBoundaryNode::recordCall);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::Button("definition-bound",
                                            this->props.emitter));
    }

  private:
    void recordCall()
    {
      ++*this->props.calls;
    }
  };

  struct PointerBindScenario
  {
    PointerBindScenario()
        : revision(0),
          propsOld(),
          propsNew(),
          definitionOld(),
          definitionNew(),
          currentProps(&this->propsOld),
          currentDefinition(&this->definitionOld),
          propsCalls(0),
          definitionCalls(0)
    {
    }

    loka::core::MutableState<int> revision;
    loka::core::EmitterState propsOld;
    loka::core::EmitterState propsNew;
    loka::core::EmitterState definitionOld;
    loka::core::EmitterState definitionNew;
    loka::core::EmitterState *currentProps;
    loka::core::EmitterState *currentDefinition;
    int propsCalls;
    int definitionCalls;
  };

  PointerBindScenario *g_pointerBindScenario = 0;

  class PropsPointerBindRootNode;
  typedef loka::app::scene::BoundaryPropsFor<PropsPointerBindRootNode>
      PropsPointerBindRootProps;

  class PropsPointerBindRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            PropsPointerBindRootNode, PropsPointerBindRootProps>
  {
  public:
    explicit PropsPointerBindRootNode(
        const PropsPointerBindRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<
              PropsPointerBindRootNode, PropsPointerBindRootProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(
        loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(
        loka::app::scene::DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(&g_pointerBindScenario->revision,
                                  loka::app::scene::NODE_DIRTY_CHILD);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      root << loka::app::scene::Boundary<PropsPointerBindBoundaryNode>(
          PropsPointerBindProps(
              g_pointerBindScenario->currentProps,
              &g_pointerBindScenario->propsCalls));
      composition.declare(root);
    }
  };

  class DefinitionPointerBindRootNode;
  typedef loka::app::scene::BoundaryPropsFor<DefinitionPointerBindRootNode>
      DefinitionPointerBindRootProps;

  class DefinitionPointerBindRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            DefinitionPointerBindRootNode, DefinitionPointerBindRootProps>
  {
  public:
    explicit DefinitionPointerBindRootNode(
        const DefinitionPointerBindRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<
              DefinitionPointerBindRootNode,
              DefinitionPointerBindRootProps>(props)
    {
    }

    virtual bool flushViewDirtyImmediately(
        loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void declareDirtySources(
        loka::app::scene::DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(&g_pointerBindScenario->revision,
                                  loka::app::scene::NODE_DIRTY_CHILD);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      root << loka::app::scene::Boundary<DefinitionPointerBindBoundaryNode>(
          DefinitionPointerBindProps(
              g_pointerBindScenario->currentDefinition,
              &g_pointerBindScenario->definitionCalls));
      composition.declare(root);
    }
  };
#ifndef LOKA_LIFECYCLE_AUDIT
  class DisarmedBindingNode;
  typedef loka::app::scene::BoundaryPropsFor<DisarmedBindingNode> DisarmedBindingProps;

  class DisarmedBindingNode : public loka::app::scene::BoundaryNodeFor<DisarmedBindingNode>
  {
  public:
    explicit DisarmedBindingNode(const DisarmedBindingProps &p = DisarmedBindingProps())
        : loka::app::scene::BoundaryNodeFor<DisarmedBindingNode>(p),
          token_(0), validCalls_(0), refusedCalls_(0) {}
    void emit()
    {
      this->valid_.emit();
      this->refused_.emit();
    }
    bool onlyDeclaredHandlerFired() const
    {
      return this->validCalls_ == 1 && this->refusedCalls_ == 0;
    }
  protected:
    virtual void declareBindings(loka::app::scene::BindingToken &t)
    {
      this->token_ = &t;
      t.action(this->valid_, this, &DisarmedBindingNode::validCall);
    }
    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      this->token_->action(this->refused_, this, &DisarmedBindingNode::refusedCall);
      this->token_->watch(this->refused_, this, &DisarmedBindingNode::refusedCall, true);
    }
  private:
    void validCall() { ++this->validCalls_; }
    void refusedCall() { ++this->refusedCalls_; }
    loka::app::scene::BindingToken *token_;
    loka::core::EmitterState valid_;
    loka::core::EmitterState refused_;
    int validCalls_;
    int refusedCalls_;
  };
#endif
} // namespace

void testAttachNodeReplayRestoresParkedBranchBindings()
{
  AttachReplayScenario scenario;
  g_attachReplayScenario = &scenario;
  {
    NullScenePlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<AttachReplayRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    scenario.guardedEmitter.emit();
    scenario.unguardedEmitter.emit();
    LOKA_VERIFY(scenario.guardedCalls == 1 &&
                scenario.unguardedCalls == 1);

    scenario.visible.set(false);
    assert(scene.hasPendingInvalidation());
    LOKA_VERIFY(scene.flushInvalidation());
    scenario.visible.set(true);
    assert(scene.hasPendingInvalidation());
    LOKA_VERIFY(scene.flushInvalidation());

    LOKA_VERIFY(scenario.guardedConstructions == 1);
    LOKA_VERIFY(scenario.guardedAttaches == 2);

    scenario.guardedEmitter.emit();
    scenario.unguardedEmitter.emit();
    LOKA_VERIFY(scenario.guardedCalls == 2 &&
                "guarded attach work cannot suppress binding declarations");
    LOKA_VERIFY(scenario.unguardedCalls == 2 &&
                "declareBindings restores the parked node's callback");
  }
  g_attachReplayScenario = 0;
}

void testPropsSuppliedEmitterBindingFollowsDefinitionRecompose()
{
  PointerBindScenario scenario;
  g_pointerBindScenario = &scenario;
  {
    NullScenePlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<PropsPointerBindRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    scenario.propsOld.emit();
    LOKA_VERIFY(scenario.propsCalls == 1);

    scenario.currentProps = &scenario.propsNew;
    scenario.revision.set(1);
    assert(scene.hasPendingInvalidation());
    LOKA_VERIFY(scene.flushInvalidation());

    scenario.propsNew.emit();
    LOKA_VERIFY(scenario.propsCalls == 2 &&
                "props-pointer binding follows the definition recompose");
    scenario.propsOld.emit();
    LOKA_VERIFY(scenario.propsCalls == 2 &&
                "props-pointer recompose releases the old emitter");
  }
  {
    NullScenePlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<DefinitionPointerBindRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    scenario.definitionOld.emit();
    LOKA_VERIFY(scenario.definitionCalls == 1);

    scenario.currentDefinition = &scenario.definitionNew;
    scenario.revision.set(2);
    assert(scene.hasPendingInvalidation());
    LOKA_VERIFY(scene.flushInvalidation());

    scenario.definitionNew.emit();
    LOKA_VERIFY(scenario.definitionCalls == 2);
    scenario.definitionOld.emit();
    LOKA_VERIFY(scenario.definitionCalls == 2 &&
                "definition recompose must release the old props pointer");
  }
  g_pointerBindScenario = 0;
}

void testDisarmedBindingTokenRefusesOutsideDeclaration()
{
#ifndef LOKA_LIFECYCLE_AUDIT
  DisarmedBindingNode node;
  loka::app::scene::ComponentContext context;
  context.setBoundary(&node);
  node.compose(context, loka::app::scene::COMPOSE_EVENT_ATTACH);
  node.emit();
  LOKA_VERIFY(node.onlyDeclaredHandlerFired());
  node.compose(context, loka::app::scene::COMPOSE_EVENT_DETACH);
#else
  std::puts("[skip] disarmed token refusal requires a non-audit build; audit asserts");
#endif
}
