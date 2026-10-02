#include "ToolboxNodeDispatch.hpp"
#include "ToolboxPlatformLayoutHandlers.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include <cstring>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  class Probe : public Node, public IProjectedLayoutNode
  {
  public:
    LayoutState offer;
    virtual IProjectedLayoutNode *asProjectedLayoutNode() { return this; }
    virtual short layoutProjected(IPlatformController *, LayoutState &state)
    {
      this->offer = state;
      state.y = static_cast<short>(state.y + 7);
      return 23;
    }
  };
}
int main(int argc, char **argv)
{
  LOKA_VERIFY(argc == 3);
  ToolboxScenePlatformController controller;
  const bool handler = std::strcmp(argv[1], "handler") == 0;
  if (handler) RegisterToolboxPlatformLayoutHandlers(controller.registry);
  const char *mode = argv[2];
  const bool nested = std::strcmp(mode, "nested") == 0;
  const bool fixed = std::strcmp(mode, "fixed") == 0;
  const bool empty = std::strcmp(mode, "empty") == 0;
  const short extent = std::strcmp(mode, "small") == 0 ? 6 :
      std::strcmp(mode, "equal") == 0 ? 10 :
      std::strcmp(mode, "zero") == 0 ? 0 :
      std::strcmp(mode, "negative") == 0 ? -3 : 80;
  StackNode column((StackProps(STACK_AXIS_COLUMN)));
  BoxProps props;
  props.setPadding(5);
  if (fixed) props.setSize(100, 60);
  BoxNode *box = new BoxNode(props);
  column.addChild(box);
  // An empty registry forces the actual dispatch fallback, including nested Boxes.
  LOKA_VERIFY((controller.registry.find(box) != 0) == handler);
  Probe *child = 0;
  if (!empty)
  {
    child = new Probe();
    if (nested)
    {
      BoxNode *inner = new BoxNode(props);
      box->addChild(inner);
      inner->addChild(child);
    }
    else box->addChild(child);
  }
  Probe *sibling = new Probe();
  column.addChild(sibling);
  LayoutState state;
  state.x = 10; state.y = 20; state.width = extent; state.height = extent;
  const short width = LayoutNode(&column, state, &controller, 0);
  const short bottom = fixed ? 80 : empty ? 30 : nested ? 47 : 37;
  if (child)
  {
    const short inset = nested ? 10 : 5;
    LOKA_VERIFY(child->offer.x == 10 + inset);
    LOKA_VERIFY(child->offer.y == 20 + inset);
    LOKA_VERIFY(child->offer.width == (fixed ? 90 : extent > 2 * inset ? extent - 2 * inset : 0));
    LOKA_VERIFY(child->offer.height == (fixed ? 50 : extent > 2 * inset ? extent - 2 * inset : 0));
  }
  LOKA_VERIFY(width == (fixed ? 100 : empty ? 23 : nested ? 43 : 33));
  LOKA_VERIFY(sibling->offer.y == bottom);
  LOKA_VERIFY(state.y == bottom + 7);
  std::printf("%s/%s passed: sibling Y=%d, width=%d\n", argv[1], mode, sibling->offer.y, width);
}
