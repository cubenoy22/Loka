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
    unsigned calls;
    const short advance;
    explicit Probe(short height = 7) : calls(0), advance(height) {}
    virtual IProjectedLayoutNode *asProjectedLayoutNode() { return this; }
    virtual short layoutProjected(IPlatformController *, LayoutState &state)
    {
      ++this->calls;
      this->offer = state;
      state.y = static_cast<short>(state.y + this->advance);
      return 23;
    }
  };

  void scrollRangePin(ToolboxScenePlatformController &controller, bool handler, bool refuses)
  {
    StackNode column((StackProps(STACK_AXIS_COLUMN)));
    BoxNode *box = new BoxNode(BoxProps().setPadding(10));
    Probe *child = new Probe(0);
    Probe *sibling = new Probe(0);
    box->addChild(child);
    column.addChild(box);
    column.addChild(sibling);
    LOKA_VERIFY((controller.registry.find(box) != 0) == handler);
    ProjectionParentScope scrollScope;
    LOKA_VERIFY(controller.projectionParentScopes_.current().deriveScrolled(
        0, 0, 0, loka::core::Frame(0, 0, 80, 80), scrollScope));
    ProjectionParentScopeGuard guard(controller.projectionParentScopes_, scrollScope);
    LOKA_VERIFY(guard.isActive());
    LayoutState state;
    state.width = 80; state.height = 80;
    // Child Y remains representable; only the Box's bottom padding overflows.
    state.y = static_cast<short>(SHRT_MAX - (refuses ? 15 : 20));
    LayoutNode(&column, state, &controller, 0);
    const bool refused = controller.projectionParentScopes_.current().hasShortRangeRefusal();
    std::printf("scroll range: refused=%d child calls=%u sibling calls=%u sibling Y=%d\n",
                refused, child->calls, sibling->calls, sibling->offer.y);
    std::fflush(stdout);
    LOKA_VERIFY(child->calls == 1);
    LOKA_VERIFY(child->offer.y == SHRT_MAX - (refuses ? 5 : 10));
    LOKA_VERIFY(refused == refuses);
    LOKA_VERIFY(sibling->calls == (refuses ? 0u : 1u));
    LOKA_VERIFY(state.y >= 0);
    if (!refuses) LOKA_VERIFY(sibling->offer.y == SHRT_MAX);
  }

}
int main(int argc, char **argv)
{
  LOKA_VERIFY(argc == 3);
  ToolboxScenePlatformController controller;
  const bool handler = std::strcmp(argv[1], "handler") == 0;
  if (handler) RegisterToolboxPlatformLayoutHandlers(controller.registry);
  const char *mode = argv[2];
  if (std::strcmp(mode, "scroll-refusal") == 0 || std::strcmp(mode, "scroll-limit") == 0)
  {
    scrollRangePin(controller, handler, std::strcmp(mode, "scroll-refusal") == 0);
    return 0;
  }
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
