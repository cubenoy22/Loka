#include "ObservedLayoutInputsTests.hpp"
#include "support/TestVerify.hpp"
#include "support/NullLayoutRefusal.hpp"
#include "testing/scene/NodeObservedUsesTestAccess.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/RectSurface.hpp"
#include "platform/null/NullScenePlatformController.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using loka::app::testing::NodeObservedUsesTestAccess;

  class InputContext : public NodeContext
  {
  public:
    InputContext() : seen(NODE_DIRTY_NONE), calls(0), markDuringLayout(false) {}
    virtual short layout(IPlatformController *, LayoutState &state)
    {
      this->seen = state.inputs;
      ++this->calls;
      if (this->markDuringLayout)
        this->owner()->requeueLayoutInputs(NODE_DIRTY_CHILD);
      return static_cast<short>(state.y + 7);
    }
    NodeDirtyFlags seen;
    unsigned calls;
    bool markDuringLayout;
  };

  class ProjectedProbe : public Node, public IProjectedLayoutNode
  {
  public:
    ProjectedProbe() : afterLayout(NODE_DIRTY_NONE)
    { this->setContext(new InputContext()); }
    virtual IProjectedLayoutNode *asProjectedLayoutNode() { return this; }
    virtual short layoutProjected(IPlatformController *controller, LayoutState &state)
    {
      const short result = this->layout(controller, state);
      this->afterLayout = state.inputs;
      return result;
    }
    InputContext &probeContext() { return *static_cast<InputContext *>(this->getContext()); }
    NodeDirtyFlags afterLayout;
  };

  LayoutState input()
  {
    LayoutState state;
    state.width = 100;
    state.height = 40;
    state.inputs = NODE_DIRTY_INITIAL;
    return state;
  }
}

void testObservedLayoutInputsRequeuedAfterProjectionRefusal()
{
  NullScenePlatformController platform;
  ProjectedProbe node;
  node.requeueLayoutInputs(NODE_DIRTY_PROPS);
  node.probeContext().markDuringLayout = true;
  const LayoutState state = input();
  loka::testing::failNullProjectedLayoutRestores(1);
  LOKA_VERIFY(platform.projectLayoutForTesting(&node, state) == state.y);
  LOKA_VERIFY(platform.scrollViewShortRangeRefusalCount() == 1);
  LOKA_VERIFY(node.probeContext().calls == 1);
  LOKA_VERIFY(node.probeContext().seen == NODE_DIRTY_PROPS);
  LOKA_VERIFY(node.afterLayout == NODE_DIRTY_INITIAL);
  const NodeDirtyFlags accumulated = static_cast<NodeDirtyFlags>(NODE_DIRTY_PROPS | NODE_DIRTY_CHILD);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(node) == accumulated);
  node.probeContext().markDuringLayout = false;
  LOKA_VERIFY(platform.projectLayoutForTesting(&node, state) == 7);
  LOKA_VERIFY(node.probeContext().seen == accumulated);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(node) == NODE_DIRTY_NONE);
  LOKA_VERIFY(state.inputs == NODE_DIRTY_INITIAL);
}

void testObservedLayoutInputsRequeuedAfterSurfaceRefusal()
{
  NullScenePlatformController platform;
  RectSurfaceNode surface((RectSurfaceProps()));
  surface.requeueLayoutInputs(NODE_DIRTY_LAYOUT);
  const LayoutState state = input();
  loka::testing::failNullProjectedLayoutRestores(1);
  LOKA_VERIFY(platform.projectLayoutForTesting(&surface, state) == state.y);
  LOKA_VERIFY(platform.scrollViewShortRangeRefusalCount() == 1);
  LOKA_VERIFY(surface.getContext() != 0);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(surface) == NODE_DIRTY_LAYOUT);
  platform.projectLayoutForTesting(&surface, state);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(surface) == NODE_DIRTY_NONE);
}

void testObservedLayoutInputsHandlerStackKeepsMark()
{
  NullScenePlatformController platform;
  StackNode stack((StackProps(STACK_AXIS_COLUMN)));
  ProjectedProbe *child = new ProjectedProbe();
  stack.addChild(child);
  stack.requeueLayoutInputs(NODE_DIRTY_LAYOUT);
  child->requeueLayoutInputs(NODE_DIRTY_PROPS);
  LOKA_VERIFY(platform.projectLayoutForTesting(&stack, input()) == 7);
  LOKA_VERIFY(child->probeContext().calls == 1);
  LOKA_VERIFY(child->probeContext().seen == NODE_DIRTY_PROPS);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(*child) == NODE_DIRTY_NONE);
  // The handler visited the child but did not pass the Stack through Node::layout.
  // K2 must choose the common dispatch door before containers consume marks.
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(stack) == NODE_DIRTY_LAYOUT);
}
