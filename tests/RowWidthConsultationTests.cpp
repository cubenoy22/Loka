#include "RowWidthConsultationTests.hpp"

#include "app/OpenFileDialog.hpp"
#include "app/RectSurface.hpp"
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
#include "core/StateTracker.hpp"
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

  template <int BoxHeight>
  class RowBranchWidthBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<RowBranchWidthBoundaryNode<BoxHeight> >
  {
  public:
    typedef loka::app::scene::BoundaryPropsFor<RowBranchWidthBoundaryNode<BoxHeight> > Props;

    explicit RowBranchWidthBoundaryNode(const Props &props)
        : loka::app::scene::BoundaryNodeFor<RowBranchWidthBoundaryNode<BoxHeight> >(props),
          fixedArmShown_()
    {
      this->state(this->fixedArmShown_, false);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      loka::app::BoxProps boxProps;
      boxProps.setSize(20, BoxHeight);
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

  template <int BoxHeight>
  void verifyRowBranchArmSwitchRelayoutsWidthConsultation()
  {
    RowTextSeatRecord record;
    g_rowTextSeatRecord = &record;
    NullScenePlatformController platform;
    loka::app::scene::Scene scene(
        (loka::app::scene::Boundary<RowBranchWidthBoundaryNode<BoxHeight> >()));
    scene.mount(&platform);
    scene.updateAttached(true);

    RowBranchWidthBoundaryNode<BoxHeight> *boundary = static_cast<RowBranchWidthBoundaryNode<BoxHeight> *>(
        loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
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
} // namespace

void testRowWidthOnlyBoxLeavesOnlyRemainingSeatForText()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::BoxProps boxProps;
  boxProps.setSize(200, 0);
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

void testRowMaterializedDialogConsumesNeitherSeatNorGap()
{
  // A Show-gated OpenFileDialog materializes as a Fragment holding the
  // dialog node while the dialog is up. The Row's width consultation must
  // treat it like an empty branch: no seat, no gap, so the visual sibling
  // keeps the whole width whether the dialog is open or not (#588).
  loka::core::EmitterState onResult;
  loka::app::OpenFileDialogProps dialogProps;
  dialogProps.onResult(&onResult);
  loka::app::FragmentNode *branch = new loka::app::FragmentNode((loka::app::FragmentProps()));
  branch->setPropsTypeId(loka::app::FragmentProps::staticTypeId());
  branch->addChild(new loka::app::OpenFileDialogNode(dialogProps));

  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  row.addChild(new RowSeatTextNode(loka::app::TextProps("text")));
  row.addChild(branch);
  row.addChild(new loka::app::OpenFileDialogNode(dialogProps));

  RowTextSeatRecord record;
  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, rowState(500));
  g_rowTextSeatRecord = 0;

  LOKA_VERIFY(record.calls == 1);
  LOKA_VERIFY(record.state.x == 0);
  LOKA_VERIFY(record.state.width == 500);
}

void testRowMaterializedDialogDoesNotRaiseAlignedRowHeight()
{
  // An aligned Row measures every child for the row height. The dialog
  // contributes no height, so a surface shorter than the fallback text
  // height keeps the row at its own height and lands at y=0 instead of
  // being centred inside a 20px row the dialog would have opened (#588).
  NullScenePlatformController platform;
  loka::core::MutableState<loka::core::Frame> extent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&extent);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  loka::app::RectSurfaceProps surfaceProps;
  surfaceProps.size(50, 8).laidOutExtent(extentState);
  loka::core::EmitterState onResult;
  loka::app::OpenFileDialogProps dialogProps;
  dialogProps.onResult(&onResult);

  loka::app::StackProps rowProps(loka::app::STACK_AXIS_ROW);
  rowProps.alignVertical(loka::app::VERTICAL_ALIGNMENT_CENTER);
  loka::app::StackNode row(rowProps);
  row.addChild(new loka::app::RectSurfaceNode(surfaceProps));
  row.addChild(new loka::app::OpenFileDialogNode(dialogProps));

  platform.projectLayoutForTesting(&row, rowState(500));

  LOKA_VERIFY(extent.get() == loka::core::Frame(0, 0, 50, 8));
}

void testRowBranchArmSwitchRelayoutsWidthConsultation()
{
  verifyRowBranchArmSwitchRelayoutsWidthConsultation<10>();
}

void testRowWidthOnlyBranchArmForwardsWidthClaim()
{
  verifyRowBranchArmSwitchRelayoutsWidthConsultation<0>();
}

void testRowHeightOnlyBoxKeepsFlexSeat()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::BoxProps boxProps;
  boxProps.setSize(0, 10);
  row.addChild(new loka::app::BoxNode(boxProps));
  row.addChild(new RowSeatTextNode(loka::app::TextProps("text")));

  RowTextSeatRecord record;
  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, rowState(500));
  g_rowTextSeatRecord = 0;

  LOKA_VERIFY(record.calls == 1);
  LOKA_VERIFY(record.state.x == 252);
  LOKA_VERIFY(record.state.width == 248);
}

void testRowWidthOnlyBoxUsesRowHeight()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::BoxProps boxProps;
  boxProps.setSize(200, 0);
  loka::app::BoxNode *box = new loka::app::BoxNode(boxProps);
  RowTextSeatRecord record;
  box->addChild(new RowSeatTextNode(loka::app::TextProps("box")));
  row.addChild(box);

  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, rowState(500));
  g_rowTextSeatRecord = 0;

  LOKA_VERIFY(record.calls == 1);
  LOKA_VERIFY(record.state.x == 0);
  LOKA_VERIFY(record.state.width == 200);
  LOKA_VERIFY(record.state.height == 20);
}

void testColumnWidthOnlyBoxKeepsFullAvailableWidth()
{
  loka::app::StackProps columnProps(loka::app::STACK_AXIS_COLUMN);
  columnProps.alignHorizontal(loka::app::HORIZONTAL_ALIGNMENT_LEADING);
  loka::app::StackNode column(columnProps);
  loka::app::BoxProps boxProps;
  boxProps.setSize(200, 0);
  loka::app::BoxNode *box = new loka::app::BoxNode(boxProps);
  RowTextSeatRecord record;
  box->addChild(new RowSeatTextNode(loka::app::TextProps("box")));
  column.addChild(box);

  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&column, rowState(500));
  g_rowTextSeatRecord = 0;

  LOKA_VERIFY(record.calls == 1);
  LOKA_VERIFY(record.state.x == 0);
  LOKA_VERIFY(record.state.width == 500);
}

void testRowOversizedFixedBoxKeepsDeclaredSeatForSibling()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::BoxProps boxProps;
  boxProps.setSize(600, 10);
  row.addChild(new loka::app::BoxNode(boxProps));
  row.addChild(new RowSeatTextNode(loka::app::TextProps("text")));

  RowTextSeatRecord record;
  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, rowState(500));
  g_rowTextSeatRecord = 0;

  LOKA_VERIFY(record.calls == 1);
  LOKA_VERIFY(record.state.x == 604);
  LOKA_VERIFY(record.state.width == 0);
}

void testRowZeroWidthRowKeepsFixedSeatLiveAndGapped()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::BoxProps boxProps;
  boxProps.setSize(200, 10);
  row.addChild(new loka::app::BoxNode(boxProps));
  row.addChild(new RowSeatTextNode(loka::app::TextProps("text")));

  RowTextSeatRecord record;
  g_rowTextSeatRecord = &record;
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, rowState(0));
  g_rowTextSeatRecord = 0;

  LOKA_VERIFY(record.calls == 1);
  LOKA_VERIFY(record.state.x == 204);
  LOKA_VERIFY(record.state.width == 0);
}

void testRowWidthHeuristicForwardsOnlySingleFragmentClaims()
{
  loka::app::ImageViewProps imageProps;
  imageProps.size(75, 20);
  loka::app::ImageViewNode *image = new loka::app::ImageViewNode(imageProps);

  loka::app::FragmentNode empty((loka::app::FragmentProps()));
  empty.setPropsTypeId(loka::app::FragmentProps::staticTypeId());
  LOKA_VERIFY(loka::app::layout::preferredChildWidthForRow(&empty) == 0);

  loka::app::FragmentNode single((loka::app::FragmentProps()));
  single.setPropsTypeId(loka::app::FragmentProps::staticTypeId());
  single.addChild(image);
  LOKA_VERIFY(loka::app::layout::preferredChildWidthForRow(&single) == 75);

  single.addChild(new loka::app::TextNode(loka::app::TextProps("text")));
  LOKA_VERIFY(loka::app::layout::preferredChildWidthForRow(&single) < 0);
}
