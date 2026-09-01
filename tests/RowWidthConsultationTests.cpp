#include "RowWidthConsultationTests.hpp"

#include "app/layout/LayoutHeuristics.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/state/NodeState.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  struct RowTextSeatRecord
  {
    RowTextSeatRecord()
        : calls(0),
          state()
    {
    }

    int calls;
    loka::app::scene::LayoutState state;
  };

  RowTextSeatRecord *g_rowTextSeatRecord = 0;

  class RowSeatTextNode : public loka::app::TextNode
  {
  public:
    typedef loka::app::TextTypeTag TypeTag;

    explicit RowSeatTextNode(const loka::app::TextProps &props)
        : loka::app::TextNode(props)
    {
    }

    virtual short layoutProjected(loka::app::scene::IPlatformController *controller,
                                  loka::app::scene::LayoutState &state)
    {
      if (g_rowTextSeatRecord)
      {
        ++g_rowTextSeatRecord->calls;
        g_rowTextSeatRecord->state = state;
      }
      return loka::app::TextNode::layoutProjected(controller, state);
    }
  };

  struct RowSeatTextDefinition : public loka::app::scene::NodeDefinition<loka::app::TextProps, RowSeatTextNode>
  {
    typedef loka::app::scene::NodeDefinition<loka::app::TextProps, RowSeatTextNode> BaseType;

    explicit RowSeatTextDefinition(const char *text)
        : BaseType(loka::app::TextProps(text))
    {
    }
  };

  loka::app::scene::LayoutState rowState(short width)
  {
    loka::app::scene::LayoutState state;
    state.width = width;
    state.height = 20;
    state.lineHeight = 20;
    return state;
  }

  class RowBranchWidthBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RowBranchWidthBoundaryNode> RowBranchWidthBoundaryProps;

  class RowBranchWidthBoundaryNode : public loka::app::scene::BoundaryNodeFor<RowBranchWidthBoundaryNode>
  {
  public:
    explicit RowBranchWidthBoundaryNode(const RowBranchWidthBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<RowBranchWidthBoundaryNode>(props),
          fixedArmShown_()
    {
      this->state(this->fixedArmShown_, false);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::BoxProps boxProps;
      boxProps.setSize(20, 10);
      loka::app::ShowDefinition fixedArm = loka::app::Show(*this->fixedArmShown_.state());
      fixedArm << loka::app::BoxDefinition(boxProps);

      loka::app::RowDefinition row;
      row << fixedArm << RowSeatTextDefinition("text");
      composition.declare(row);
    }

    void showFixedArm()
    {
      this->fixedArmShown_.set(true);
    }

  private:
    loka::app::scene::NodeState<bool> fixedArmShown_;
  };
} // namespace

void testRowFixedBoxWidthLeavesOnlyRemainingSeatForText()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::BoxProps boxProps;
  boxProps.setSize(200, 10);
  row.addChild(new loka::app::BoxNode(boxProps));
  row.addChild(new RowSeatTextNode(loka::app::TextProps("text")));

  RowTextSeatRecord record;
  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, rowState(500));
  g_rowTextSeatRecord = 0;

  LOKA_VERIFY(record.calls == 1);
  LOKA_VERIFY(record.state.x == 204);
  LOKA_VERIFY(record.state.width == 296);
}

void testRowEmptyFragmentConsumesNeitherSeatNorGap()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::FragmentNode *empty = new loka::app::FragmentNode((loka::app::FragmentProps()));
  empty->setPropsTypeId(loka::app::FragmentProps::staticTypeId());
  row.addChild(empty);
  row.addChild(new RowSeatTextNode(loka::app::TextProps("text")));

  RowTextSeatRecord record;
  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, rowState(500));
  g_rowTextSeatRecord = 0;

  LOKA_VERIFY(record.calls == 1);
  LOKA_VERIFY(record.state.x == 0);
  LOKA_VERIFY(record.state.width == 500);
}

void testRowBranchArmSwitchRelayoutsWidthConsultation()
{
  RowTextSeatRecord record;
  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  loka::app::scene::Scene scene((loka::app::scene::Boundary<RowBranchWidthBoundaryNode>()));
  scene.mount(&platform);
  scene.updateAttached(true);

  RowBranchWidthBoundaryNode *boundary =
      static_cast<RowBranchWidthBoundaryNode *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(boundary != 0);
  const unsigned long layoutCalls = platform.onChangeCallCount();
  const int textCalls = record.calls;

  boundary->showFixedArm();
  if (scene.hasPendingInvalidation())
  {
    LOKA_VERIFY(scene.flushInvalidation());
  }

  LOKA_VERIFY(platform.onChangeCallCount() == layoutCalls + 1);
  LOKA_VERIFY((platform.lastOnChangeFlags() & loka::app::scene::NODE_DIRTY_CHILD) != 0);
  LOKA_VERIFY((platform.lastOnChangeFlags() & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);
  LOKA_VERIFY(record.calls == textCalls + 1);
  LOKA_VERIFY(record.state.x == 24);
  LOKA_VERIFY(record.state.width == 76);

  scene.unmount();
  g_rowTextSeatRecord = 0;
}

void testRowWidthHeuristicForwardsOnlySingleFragmentClaims()
{
  loka::app::ImageViewProps imageProps;
  imageProps.size(75, 20);
  loka::app::ImageViewNode *image = new loka::app::ImageViewNode(imageProps);

  loka::app::FragmentNode empty((loka::app::FragmentProps()));
  empty.setPropsTypeId(loka::app::FragmentProps::staticTypeId());
  LOKA_VERIFY(loka::app::layout::preferredChildWidthForRow(&empty, 200) == 0);

  loka::app::FragmentNode single((loka::app::FragmentProps()));
  single.setPropsTypeId(loka::app::FragmentProps::staticTypeId());
  single.addChild(image);
  LOKA_VERIFY(loka::app::layout::preferredChildWidthForRow(&single, 200) == 75);

  single.addChild(new loka::app::TextNode(loka::app::TextProps("text")));
  LOKA_VERIFY(loka::app::layout::preferredChildWidthForRow(&single, 200) < 0);
}
