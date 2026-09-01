#include "LocalRecomposeRefusalTests.hpp"

#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/LimitedCloneProbe.hpp"
#include "support/RecomposingBoundary.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  class AttachObservableBoundaryNode;
  struct AttachObservableBoundaryTypeTag
  {
  };

  struct AttachObservableBoundaryProps
      : public loka::app::scene::NodePropsBase<AttachObservableBoundaryProps>
  {
    typedef AttachObservableBoundaryTypeTag TypeTag;
    typedef AttachObservableBoundaryNode NodeType;

    explicit AttachObservableBoundaryProps(int policy)
        : policy_(policy)
    {
    }

    int policy() const
    {
      return this->policy_;
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return this->propsTypeId() < rhs.propsTypeId();
      }
      return this->policy_ <
             static_cast<const AttachObservableBoundaryProps &>(rhs).policy_;
    }

  private:
    int policy_;
  };

  class AttachObservableBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<
            AttachObservableBoundaryProps>
  {
  public:
    explicit AttachObservableBoundaryNode(
        const AttachObservableBoundaryProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<
              AttachObservableBoundaryProps>(props)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      typedef loka::app::scene::NodeDefinition<
          DefinitionCloneTestSupport::LimitedClonePolicyProbeProps,
          DefinitionCloneTestSupport::LimitedClonePolicyProbeNode>
          ProbeDefinition;
      ProbeDefinition child(
          DefinitionCloneTestSupport::LimitedClonePolicyProbeProps(
              this->props.policy()));
      composition.declare(child);
    }
  };

  struct LimitedCloneAttachObservableBoundaryDefinition
      : public loka::app::scene::BoundaryDefinition<
            AttachObservableBoundaryProps,
            AttachObservableBoundaryNode>
  {
    typedef loka::app::scene::BoundaryDefinition<
        AttachObservableBoundaryProps,
        AttachObservableBoundaryNode>
        BaseType;

    explicit LimitedCloneAttachObservableBoundaryDefinition(int policy)
        : BaseType(AttachObservableBoundaryProps(policy))
    {
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      if (!DefinitionCloneTestSupport::limitedCloneBudgetAllowsClone())
      {
        return 0;
      }
      return new LimitedCloneAttachObservableBoundaryDefinition(*this);
    }
  };

  class RefusingLocalRecomposeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RefusingLocalRecomposeBoundaryNode>
      RefusingLocalRecomposeBoundaryProps;

  class RefusingLocalRecomposeBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            RefusingLocalRecomposeBoundaryNode,
            RefusingLocalRecomposeBoundaryProps,
            true>
  {
  public:
    explicit RefusingLocalRecomposeBoundaryNode(
        const RefusingLocalRecomposeBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<
              RefusingLocalRecomposeBoundaryNode,
              RefusingLocalRecomposeBoundaryProps,
              true>(props),
          policy_(0)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      LimitedCloneAttachObservableBoundaryDefinition child(
          this->policy_);
      composition.declare(child);
    }

    virtual bool flushViewDirtyImmediately(
        loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    void flipPolicy()
    {
      this->policy_ = 1;
      this->markViewDirty(loka::app::scene::NODE_DIRTY_CHILD);
    }

  private:
    int policy_;
  };

  AttachObservableBoundaryNode *onlyAttachObservableBoundary(
      RefusingLocalRecomposeBoundaryNode &boundary)
  {
    loka::app::scene::Node *child = boundary.childrenHead();
    LOKA_VERIFY(child != 0);
    LOKA_VERIFY(child->nextInComposition == 0);
    return static_cast<AttachObservableBoundaryNode *>(child);
  }

  DefinitionCloneTestSupport::LimitedClonePolicyProbeNode *onlyNestedProbeChild(
      RefusingLocalRecomposeBoundaryNode &boundary)
  {
    AttachObservableBoundaryNode *nested =
        onlyAttachObservableBoundary(boundary);
    loka::app::scene::Node *child = nested->childrenHead();
    LOKA_VERIFY(child != 0);
    LOKA_VERIFY(child->nextInComposition == 0);
    return static_cast<DefinitionCloneTestSupport::LimitedClonePolicyProbeNode *>(child);
  }
} // namespace

void testRefusedLocalRecomposeNeverCompletesWithStaleRetainedProps()
{
  using namespace DefinitionCloneTestSupport;

  NullScenePlatformController platform;
  g_limitedCloneBudget = -1;
  g_limitedCloneCalls = 0;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<RefusingLocalRecomposeBoundaryNode>()));
  scene.mount(&platform);
  scene.updateAttached(true);

  RefusingLocalRecomposeBoundaryNode *root =
      static_cast<RefusingLocalRecomposeBoundaryNode *>(
          loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(root != 0);
  LOKA_VERIFY(onlyAttachObservableBoundary(*root)->props.policy() == 0);
  LOKA_VERIFY(onlyNestedProbeChild(*root)->props.policy() == 0);

  g_limitedCloneBudget = 1;
  const int callsBefore = g_limitedCloneCalls;
  root->flipPolicy();
  const bool projected = scene.flushInvalidation();
  (void)projected;
  g_limitedCloneBudget = -1;
  LOKA_VERIFY(g_limitedCloneCalls >= callsBefore + 2);
  LOKA_VERIFY(onlyNestedProbeChild(*root)->props.policy() == 1);

  const bool completed = root->composeResult().composed;
  LOKA_VERIFY(!completed ||
              onlyAttachObservableBoundary(*root)->props.policy() == 1);
  if (!completed)
  {
    LOKA_VERIFY(
        loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
  }
  const bool healed = root->composeResult().composed;
  LOKA_VERIFY(healed);
  LOKA_VERIFY(onlyAttachObservableBoundary(*root)->props.policy() == 1);
  LOKA_VERIFY(onlyNestedProbeChild(*root)->props.policy() == 1);

  scene.unmount();
  g_limitedCloneBudget = -1;
}
