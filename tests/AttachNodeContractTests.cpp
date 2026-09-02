#include "AttachNodeContractTests.hpp"
#include "support/TestVerify.hpp"

#include <cassert>

#include "app/nodes/boundary/StdComposition.hpp"
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

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      ++g_attachReplayScenario->guardedAttaches;
      if (this->initialized_)
      {
        return;
      }
      this->bindActionForUi(g_attachReplayScenario->guardedEmitter,
                            &GuardedAttachComponentNode::recordCall);
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

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      this->bindActionForUi(g_attachReplayScenario->unguardedEmitter,
                            &UnguardedAttachComponentNode::recordCall);
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

  class LegacyPointerBindBoundaryNode;
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

  typedef PointerBindProps<LegacyPointerBindBoundaryNode>
      LegacyPointerBindProps;
  typedef PointerBindProps<DefinitionPointerBindBoundaryNode>
      DefinitionPointerBindProps;

  class LegacyPointerBindBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<LegacyPointerBindProps>
  {
  public:
    explicit LegacyPointerBindBoundaryNode(const LegacyPointerBindProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<LegacyPointerBindProps>(props)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &composition)
    {
      (void)composition;
      this->bindActionForUi(*this->props.emitter,
                            &LegacyPointerBindBoundaryNode::recordCall);
    }

  private:
    void recordCall()
    {
      ++*this->props.calls;
    }
  };

  template <class NodeT, class PropsT>
  class PropsRecomposingBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<PropsT>
  {
  public:
    explicit PropsRecomposingBoundaryNode(const PropsT &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<PropsT>(props)
    {
    }

  protected:
    virtual void declareLocalRecomposition(
        loka::app::scene::NodeComposition &composition)
    {
      this->composeNode(composition);
    }

    virtual void composeWithContext(
        loka::app::scene::ComponentContext &context,
        loka::app::scene::ComposeEvent event)
    {
      typedef loka::app::scene::StdCompositionBoundaryNodeBase<PropsT>
          BaseType;
      if (event != loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        BaseType::composeWithContext(context, event);
        return;
      }
      this->recomposeLocalCompositionWithFullFallback(
          context, event,
          this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS);
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

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      this->bindActionForUi(*this->props.emitter,
                            &DefinitionPointerBindBoundaryNode::recordCall);
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
          legacyOld(),
          legacyNew(),
          definitionOld(),
          definitionNew(),
          currentLegacy(&this->legacyOld),
          currentDefinition(&this->definitionOld),
          legacyCalls(0),
          definitionCalls(0)
    {
    }

    loka::core::MutableState<int> revision;
    loka::core::EmitterState legacyOld;
    loka::core::EmitterState legacyNew;
    loka::core::EmitterState definitionOld;
    loka::core::EmitterState definitionNew;
    loka::core::EmitterState *currentLegacy;
    loka::core::EmitterState *currentDefinition;
    int legacyCalls;
    int definitionCalls;
  };

  PointerBindScenario *g_pointerBindScenario = 0;

  class LegacyPointerBindRootNode;
  typedef loka::app::scene::BoundaryPropsFor<LegacyPointerBindRootNode>
      LegacyPointerBindRootProps;

  class LegacyPointerBindRootNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            LegacyPointerBindRootNode, LegacyPointerBindRootProps>
  {
  public:
    explicit LegacyPointerBindRootNode(
        const LegacyPointerBindRootProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<
              LegacyPointerBindRootNode, LegacyPointerBindRootProps>(props)
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
      root << loka::app::scene::Boundary<LegacyPointerBindBoundaryNode>(
          LegacyPointerBindProps(
              g_pointerBindScenario->currentLegacy,
              &g_pointerBindScenario->legacyCalls));
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
    LOKA_VERIFY(scenario.guardedCalls == 1 &&
                "the guarded fixture is the legacy negative control");
    LOKA_VERIFY(scenario.unguardedCalls == 2 &&
                "idempotent attachNode restores the parked node's callback");
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
        (loka::app::scene::Boundary<LegacyPointerBindRootNode>()));
    scene.mount(&platform);
    scene.updateAttached(true);

    scenario.legacyOld.emit();
    LOKA_VERIFY(scenario.legacyCalls == 1);

    scenario.currentLegacy = &scenario.legacyNew;
    scenario.revision.set(1);
    assert(scene.hasPendingInvalidation());
    LOKA_VERIFY(scene.flushInvalidation());

    scenario.legacyNew.emit();
    LOKA_VERIFY(scenario.legacyCalls == 1 &&
                "attachNode remains bound to the legacy props pointer");
    scenario.legacyOld.emit();
    LOKA_VERIFY(scenario.legacyCalls == 2);
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
