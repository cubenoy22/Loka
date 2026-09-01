#include "LocalRecomposeRefusalTests.hpp"

#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/LimitedCloneProbe.hpp"
#include "support/RecomposingBoundary.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
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
      DefinitionCloneTestSupport::LimitedClonePolicyProbeDefinition child(
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

  DefinitionCloneTestSupport::LimitedClonePolicyProbeNode *onlyProbeChild(
      RefusingLocalRecomposeBoundaryNode &boundary)
  {
    loka::app::scene::Node *child = boundary.childrenHead();
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
  LOKA_VERIFY(onlyProbeChild(*root)->props.policy() == 0);

  g_limitedCloneBudget = 1;
  const int callsBefore = g_limitedCloneCalls;
  root->flipPolicy();
  const bool projected = scene.flushInvalidation();
  (void)projected;
  g_limitedCloneBudget = -1;
  LOKA_VERIFY(g_limitedCloneCalls >= callsBefore + 2);

  const bool completed = root->composeResult().composed;
  LOKA_VERIFY(!completed || onlyProbeChild(*root)->props.policy() == 1);
  if (!completed)
  {
    LOKA_VERIFY(
        loka::dsl::testing::SceneTestAccess::whiteFlagFullRebuildPending(scene));
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
    LOKA_VERIFY(scene.flushInvalidation());
  }
  const bool healed = root->composeResult().composed;
  LOKA_VERIFY(healed);
  LOKA_VERIFY(onlyProbeChild(*root)->props.policy() == 1);

  scene.unmount();
  g_limitedCloneBudget = -1;
}
