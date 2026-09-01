#include "StackAxisRetentionTests.hpp"

#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/RecomposingBoundary.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  class StackAxisRetentionBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<StackAxisRetentionBoundaryNode>
      StackAxisRetentionBoundaryProps;

  class StackAxisRetentionBoundaryNode
      : public SceneTestSupport::RecomposingBoundaryNode<
            StackAxisRetentionBoundaryNode,
            StackAxisRetentionBoundaryProps,
            true>
  {
  public:
    explicit StackAxisRetentionBoundaryNode(
        const StackAxisRetentionBoundaryProps &props)
        : SceneTestSupport::RecomposingBoundaryNode<
              StackAxisRetentionBoundaryNode,
              StackAxisRetentionBoundaryProps,
              true>(props),
          axis_(loka::app::STACK_AXIS_ROW)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::Fragment root;
      if (this->axis_ == loka::app::STACK_AXIS_ROW)
      {
        root << (loka::app::HStack() << loka::app::Text("x"));
        composition.declare(root);
        return;
      }
      root << (loka::app::VStack() << loka::app::Text("x"));
      composition.declare(root);
    }

    void setAxis(loka::app::StackAxis axis)
    {
      this->axis_ = axis;
    }

    loka::app::StackNode *stack() const
    {
      loka::app::scene::Node *root = this->compositionRootNode();
      loka::app::scene::INestable *nestable = root ? root->asNestable() : 0;
      loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
      return child ? child->asStackNode() : 0;
    }

  private:
    loka::app::StackAxis axis_;
  };
} // namespace

void testStackAxisFlipRetainsContainerAndChildAndRelayouts()
{
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<StackAxisRetentionBoundaryNode>()));
  scene.mount(&platform);
  scene.updateAttached(true);

  StackAxisRetentionBoundaryNode *boundary =
      static_cast<StackAxisRetentionBoundaryNode *>(
          loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(boundary != 0);
  loka::app::StackNode *stack = boundary->stack();
  LOKA_VERIFY(stack != 0);
  LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_ROW);
  loka::app::scene::Node *text = stack->childrenHead();
  LOKA_VERIFY(text != 0);
  LOKA_VERIFY(text->kind() == loka::app::scene::NODE_KIND_TEXT);
  const unsigned long layoutCount = platform.onChangeCallCount();

  boundary->setAxis(loka::app::STACK_AXIS_COLUMN);
  scene.requestInvalidate(static_cast<loka::app::scene::NodeDirtyFlags>(
      loka::app::scene::NODE_DIRTY_CHILD |
      loka::app::scene::NODE_DIRTY_LAYOUT));
  LOKA_VERIFY(scene.flushInvalidation());

  LOKA_VERIFY(boundary->stack() == stack);
  LOKA_VERIFY(stack->props.axis_ == loka::app::STACK_AXIS_COLUMN);
  LOKA_VERIFY(stack->childrenHead() == text);
  LOKA_VERIFY(platform.onChangeCallCount() == layoutCount + 1);
  LOKA_VERIFY((platform.lastOnChangeFlags() &
               loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

  scene.unmount();
}
