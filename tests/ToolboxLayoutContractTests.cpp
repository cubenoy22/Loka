#include "ToolboxLayoutContractTests.hpp"

#include "support/TestVerify.hpp"

#include "../apple/toolbox/src/ToolboxPlatformLayoutHandlers.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/RowColumn.hpp"

namespace
{
  class ToolboxLayoutProbeNode : public loka::app::scene::Node
  {
  };

  class ToolboxLayoutContractTraversal : public loka::app::scene::IPlatformLayoutTraversal
  {
  public:
    ToolboxLayoutContractTraversal(short childWidth, short childHeight)
        : childWidth_(childWidth),
          childHeight_(childHeight),
          callCount_(0),
          layoutResultY_(0),
          lastState_()
    {
    }

    virtual int layoutChild(loka::app::scene::Node *, const loka::app::scene::LayoutState &state)
    {
      ++this->callCount_;
      this->lastState_ = state;
      this->layoutResultY_ = static_cast<short>(state.y + this->childHeight_);
      return this->childWidth_;
    }

    virtual void setLayoutResultY(short y)
    {
      this->layoutResultY_ = y;
    }

    virtual short layoutResultY() const
    {
      return this->layoutResultY_;
    }

    short childWidth_;
    short childHeight_;
    int callCount_;
    short layoutResultY_;
    loka::app::scene::LayoutState lastState_;
  };

  loka::app::scene::LayoutState FixedBoxInputState()
  {
    loka::app::scene::LayoutState state;
    state.x = 10;
    state.y = 20;
    state.width = 80;
    state.height = 40;
    state.lineHeight = 10;
    return state;
  }

} // namespace

void testToolboxFixedBoxLayoutCommitsDeclaredExtent()
{
  loka::app::BoxProps props;
  props.setPadding(5).setSize(300, 170);
  loka::app::BoxNode box(props);
  box.addChild(new ToolboxLayoutProbeNode());
  loka::app::scene::PlatformLayoutHandlerRegistry registry;
  RegisterToolboxPlatformLayoutHandlers(registry);
  ToolboxLayoutContractTraversal traversal(23, 7);
  loka::app::scene::LayoutState state = FixedBoxInputState();
  short width = 0;

  const bool usedHandler =
      ApplyToolboxPlatformLayoutHandler(registry, box, state, traversal, width);

  LOKA_VERIFY(usedHandler);
  LOKA_VERIFY(width == 300);
  LOKA_VERIFY(traversal.callCount_ == 1);
  LOKA_VERIFY(traversal.lastState_.x == 15);
  LOKA_VERIFY(traversal.lastState_.y == 25);
  LOKA_VERIFY(traversal.lastState_.width == 290);
  LOKA_VERIFY(traversal.lastState_.height == 160);
  LOKA_VERIFY(traversal.layoutResultY() == 190);
  LOKA_VERIFY(state.y == 190);
}

void testToolboxEmptyFixedBoxLayoutCommitsDeclaredExtent()
{
  loka::app::BoxProps props;
  props.setPadding(5).setSize(300, 170);
  loka::app::BoxNode box(props);
  loka::app::scene::PlatformLayoutHandlerRegistry registry;
  RegisterToolboxPlatformLayoutHandlers(registry);
  ToolboxLayoutContractTraversal traversal(23, 7);
  loka::app::scene::LayoutState state = FixedBoxInputState();
  short width = 0;

  const bool usedHandler =
      ApplyToolboxPlatformLayoutHandler(registry, box, state, traversal, width);

  LOKA_VERIFY(usedHandler);
  LOKA_VERIFY(width == 300);
  LOKA_VERIFY(traversal.callCount_ == 0);
  LOKA_VERIFY(traversal.layoutResultY() == 190);
  LOKA_VERIFY(state.y == 190);
}

void testToolboxRowConsultsFixedChildWidth()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::BoxProps boxProps;
  boxProps.setSize(200, 10);
  row.addChild(new loka::app::BoxNode(boxProps));
  row.addChild(new ToolboxLayoutProbeNode());
  loka::app::scene::PlatformLayoutHandlerRegistry registry;
  RegisterToolboxPlatformLayoutHandlers(registry);
  ToolboxLayoutContractTraversal traversal(23, 7);
  loka::app::scene::LayoutState state = FixedBoxInputState();
  state.width = 500;
  state.spacing = 4;
  short width = 0;

  const bool usedHandler = ApplyToolboxPlatformLayoutHandler(registry, row, state, traversal, width);

  LOKA_VERIFY(usedHandler);
  LOKA_VERIFY(width == 500);
  LOKA_VERIFY(traversal.callCount_ == 2);
  LOKA_VERIFY(traversal.lastState_.x == 214);
  LOKA_VERIFY(traversal.lastState_.width == 296);
}

void testToolboxRowEmptyFragmentConsumesNoSeatOrGap()
{
  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  loka::app::FragmentNode *empty = new loka::app::FragmentNode((loka::app::FragmentProps()));
  empty->setPropsTypeId(loka::app::FragmentProps::staticTypeId());
  row.addChild(empty);
  row.addChild(new ToolboxLayoutProbeNode());
  loka::app::scene::PlatformLayoutHandlerRegistry registry;
  RegisterToolboxPlatformLayoutHandlers(registry);
  ToolboxLayoutContractTraversal traversal(23, 7);
  loka::app::scene::LayoutState state = FixedBoxInputState();
  state.width = 500;
  state.spacing = 4;
  short width = 0;

  const bool usedHandler = ApplyToolboxPlatformLayoutHandler(registry, row, state, traversal, width);

  LOKA_VERIFY(usedHandler);
  LOKA_VERIFY(width == 500);
  LOKA_VERIFY(traversal.callCount_ == 2);
  LOKA_VERIFY(traversal.lastState_.x == 10);
  LOKA_VERIFY(traversal.lastState_.width == 500);
}
