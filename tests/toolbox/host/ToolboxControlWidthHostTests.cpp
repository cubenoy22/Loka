#include "app/layout/ColumnLayout.hpp"
#include "app/layout/RowLayout.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/TestVerify.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include <cstdio>
#include <cstring>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  class WidthNode;
  typedef BoundaryPropsFor<WidthNode> WidthProps;
  class WidthNode : public StdCompositionBoundaryNodeBase<WidthProps>
  {
  public:
    typedef WidthProps::TypeTag TypeTag;
    explicit WidthNode(const WidthProps &p) : StdCompositionBoundaryNodeBase<WidthProps>(p)
    { this->state(this->title, String::Literal("Flag mode: off")); }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Column().TEST_ID("column") << Button(this->title.state()).TEST_ID("button")
          << EditText(this->title).TEST_ID("edit") << PopupMenu().TEST_ID("popup")
          << (Row().TEST_ID("row") << Button(this->title.state()).TEST_ID("row-button")));
    }
    NodeState<String> title;
  };
  Node *lookup(Scene &scene, const char *id)
  {
    Node *node = 0; loka::dsl::FlowError error;
    LOKA_VERIFY(loka::dsl::testing::LookupNodeById<Node>(&scene, id, node, error) == loka::dsl::FLOW_STEP_SUCCEEDED);
    LOKA_VERIFY(node != 0);
    return node;
  }
  LayoutState seat(short width)
  {
    LayoutState s; s.x = 10; s.y = 20; s.width = width; s.height = 24; s.spacing = 0;
    return s;
  }
  struct LayoutProbe
  {
    ToolboxScenePlatformController &controller;
    Node *target;
    short used;
    LayoutProbe(ToolboxScenePlatformController &c, Node *n) : controller(c), target(n), used(0) {}
    static int child(void *data, Node *node, const LayoutState &offer)
    {
      LayoutProbe &p = *static_cast<LayoutProbe *>(data);
      LayoutState s = offer;
      if (node == p.target)
      {
        p.used = node->context->layout(&p.controller, s);
        node->context->render(&p.controller);
      }
      return s.y;
    }
  };
}
int main(int argc, char **argv)
{
  LOKA_VERIFY(argc == 2);
  const char *mode = argv[1];
  ToolboxWindow window; ToolboxScenePlatformController controller(&window);
  Scene scene((Boundary<WidthNode>(WidthProps())));
  scene.mount(&controller);
  typedef loka::dsl::testing::SceneTestAccess Access;
  Access::updateAttached(scene, true);
  WidthNode *root = static_cast<WidthNode *>(Access::rootBoundary(scene));
  Node *button = lookup(scene, "button"), *edit = lookup(scene, "edit"), *popup = lookup(scene, "popup");
  Node *rowButton = lookup(scene, "row-button");
  button->setContext(new ToolboxButtonContext(button->asButtonNode(), &controller));
  edit->setContext(new ToolboxEditTextContext(edit->asEditTextNode(), &controller));
  popup->setContext(new ToolboxPopupMenuContext(popup->asPopupMenuNode(), &controller));
  rowButton->setContext(new ToolboxButtonContext(rowButton->asButtonNode(), &controller));
  const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
  if (std::strncmp(mode, "column-", 7) == 0)
  {
    Node *target = std::strcmp(mode, "column-button") == 0 ? button :
        std::strcmp(mode, "column-edit") == 0 ? edit : popup;
    LayoutProbe p(controller, target);
    layout::computeColumnLayoutResultY(lookup(scene, "column")->asStackNode(), seat(240), &p, LayoutProbe::child);
    std::printf("%s: used=%d outer=%d expected=240\n", mode, p.used,
        toolbox_host::controlRect.right - toolbox_host::controlRect.left); std::fflush(stdout);
    LOKA_VERIFY(p.used == 240);
    LOKA_VERIFY(toolbox_host::controlRect.right - toolbox_host::controlRect.left == 240);
  }
  else if (std::strcmp(mode, "natural") == 0)
  {
    Node *nodes[] = {button, edit, popup};
    const int widths[] = {controller.measureTextWidth(root->title.get()) + 16, 120, 120};
    const int insets[] = {0, 3, 8};
    for (int offer = 0; offer >= -1; --offer)
      for (int i = 0; i < 3; ++i)
      {
        LayoutState s = seat(static_cast<short>(offer));
        LOKA_VERIFY(nodes[i]->context->layout(&controller, s) == widths[i]);
        nodes[i]->context->render(&controller);
        LOKA_VERIFY(toolbox_host::controlRect.right - toolbox_host::controlRect.left == widths[i] + insets[i]);
      }
  }
  else
  {
    const bool natural = std::strcmp(mode, "natural-title") == 0;
    const bool clipped = std::strcmp(mode, "clipped-title") == 0;
    const short width = natural ? 0 : std::strcmp(mode, "equal-title") == 0
        ? static_cast<short>(controller.measureTextWidth(root->title.get()) + 16) : 240;
    if (clipped) controller.projectionClip.right = 5;
    LayoutProbe p(controller, rowButton);
    if (natural)
      LayoutProbe::child(&p, rowButton, seat(0));
    else
      layout::computeRowLayoutResultY(lookup(scene, "row")->asStackNode(), seat(width),
          layout::RowLayoutMetrics(), &p, LayoutProbe::child);
    ToolboxButtonContext *context = static_cast<ToolboxButtonContext *>(rowButton->context);
    const Rect before = toolbox_host::controlRect;
    { StateTrackerGuard guard(root->tracker()); root->title.set(String::Literal("Flag mode: on")); }
    const PaintAnswer answer = context->queryPaintDamage(query);
    std::printf("%s: answer=%d rect-width=%d\n", mode, answer.kind, before.right - before.left); std::fflush(stdout);
    LOKA_VERIFY(answer.kind == (natural ? PAINT_ANSWER_REFUSED : PAINT_ANSWER_EXACT));
    if (!natural && !clipped)
    {
      LOKA_VERIFY(answer.damage.x == before.left && answer.damage.y == before.top);
      LOKA_VERIFY(answer.damage.width == before.right - before.left);
      LOKA_VERIFY(answer.damage.height == before.bottom - before.top);
      LayoutState s = seat(width); context->layout(&controller, s); context->draw(&controller);
      LOKA_VERIFY(EqualRect(&before, &toolbox_host::controlRect));
    }
  }
  Access::unmount(scene);
  std::puts("Control width pin passed");
}
