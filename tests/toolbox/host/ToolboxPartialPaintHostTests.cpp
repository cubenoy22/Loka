#include "context/ToolboxTextContext.hpp"
#include "context/ToolboxAttributedTextContext.hpp"
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
  class PaintNode;
  typedef BoundaryPropsFor<PaintNode> PaintProps;
  class PaintNode : public StdCompositionBoundaryNodeBase<PaintProps>
  {
  public:
    typedef PaintProps::TypeTag TypeTag;
    explicit PaintNode(const PaintProps &p) : StdCompositionBoundaryNodeBase<PaintProps>(p)
    {
      this->state(this->text, String::Literal("V"));
      this->state(this->enabled, true);
      this->state(this->selection, 0);
      this->state(this->attributed, AttributedString(Styled("V", Bold)));
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Column() << Cell(this->text.state()).TEST_ID("cell")
          << Text(this->text.state()).TEST_ID("text")
          << Button(this->text.state()).enabled(this->enabled.state()).TEST_ID("button")
          << PopupMenu().selectedIndex(this->selection).enabled(this->enabled.state()).TEST_ID("popup")
          << AttributedText(this->attributed.state()).TEST_ID("attributed"));
    }
    NodeState<String> text;
    NodeState<bool> enabled;
    NodeState<int> selection;
    NodeState<AttributedString> attributed;
  };
  LayoutState Seat()
  {
    LayoutState seat;
    seat.x = 10; seat.y = 40; seat.width = 80; seat.height = 24; seat.lineHeight = 16; seat.spacing = 0;
    return seat;
  }
  void ExactEmpty(ToolboxProjectedNodeContext &context, const PaintQuery &query)
  {
    const PaintAnswer answer = context.queryPaintDamage(query);
    std::printf("answer kind=%d reason=%d damage=%dx%d\n", answer.kind, answer.reason,
                answer.damage.width, answer.damage.height);
    std::fflush(stdout);
    LOKA_VERIFY(answer.kind == PAINT_ANSWER_EXACT);
    LOKA_VERIFY(answer.damage.width == 0 && answer.damage.height == 0);
  }
  void Draw(ToolboxProjectedNodeContext &context, ToolboxScenePlatformController &controller, const char *mode,
            HostControl &control, std::string &label)
  {
    if (std::strcmp(mode, "button-repaint") == 0)
      static_cast<ToolboxButtonContext &>(context).repaint(&control, label);
    else if (std::strcmp(mode, "text-repaint") == 0)
      static_cast<ToolboxTextContext &>(context).repaint();
    else if (std::strcmp(mode, "popup-repaint") == 0)
      static_cast<ToolboxPopupMenuContext &>(context).repaint();
    else
      context.render(&controller);
  }
}
int main(int argc, char **argv)
{
  LOKA_VERIFY(argc == 2);
  const char *mode = argv[1];
  const std::string id(mode, std::strcspn(mode, "-"));
  ToolboxWindow window;
  ToolboxScenePlatformController controller(&window);
  Scene scene((Boundary<PaintNode>(PaintProps())));
  scene.mount(&controller);
  typedef loka::dsl::testing::SceneTestAccess Access;
  Access::updateAttached(scene, true);
  PaintNode &root = *static_cast<PaintNode *>(Access::rootBoundary(scene));
  Node *node = 0;
  loka::dsl::FlowError error;
  LOKA_VERIFY(loka::dsl::testing::LookupNodeById<Node>(&scene, id.c_str(), node, error));
  ToolboxProjectedNodeContext *context = 0;
  if (id == "cell") context = new ToolboxCellContext(node->asCellNode(), &controller);
  if (id == "text") context = new ToolboxTextContext(node->asTextNode(), &controller);
  if (id == "button") context = new ToolboxButtonContext(node->asButtonNode(), &controller);
  if (id == "popup") context = new ToolboxPopupMenuContext(node->asPopupMenuNode(), &controller);
  if (id == "attributed") context = new ToolboxAttributedTextContext(node->asAttributedTextNode(), &controller);
  LOKA_VERIFY(context);
  node->setContext(context);
  toolbox_host::reset();
  LayoutState seat = Seat();
  context->layout(&controller, seat);
  HostControl control = {0};
  std::string installedLabel;
  const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
  Draw(*context, controller, mode, control, installedLabel);
  ExactEmpty(*context, query);
  const Rect partial = {40, 10, 45, 30};
  std::printf("%s unchanged partial\n", mode); std::fflush(stdout);
  { ToolboxPaintClip clip(partial); Draw(*context, controller, mode, control, installedLabel); }
  ExactEmpty(*context, query);
  // Change the drawer's value without changing placement. AttributedText must
  // rebuild its table via layout, which conservatively revokes the old fact.
  {
    StateTrackerGuard guard(root.tracker());
    root.text.set(String::Literal("W"));
    root.selection.set(1);
    root.attributed.set(AttributedString(Styled("W", Bold)));
  }
  if (id == "attributed" || (id == "button" && std::strcmp(mode, "button") == 0))
  {
    seat = Seat(); seat.inputs = NODE_DIRTY_PROPS; context->layout(&controller, seat);
  }
  std::printf("%s changed partial\n", mode); std::fflush(stdout);
  { ToolboxPaintClip clip(partial); Draw(*context, controller, mode, control, installedLabel); }
  const PaintAnswer changed = context->queryPaintDamage(query);
  LOKA_VERIFY(changed.kind == PAINT_ANSWER_REFUSED);
  LOKA_VERIFY(changed.reason == PAINT_REFUSED_HISTORY_UNKNOWN);
  Draw(*context, controller, mode, control, installedLabel);
  ExactEmpty(*context, query);
  // Native button enabled and popup face enabled are independent value inputs.
  if (id == "button" || id == "popup")
  {
    { StateTrackerGuard guard(root.tracker()); root.enabled.set(false); }
    { ToolboxPaintClip clip(partial); Draw(*context, controller, mode, control, installedLabel); }
    const PaintAnswer disabled = context->queryPaintDamage(query);
    LOKA_VERIFY(disabled.kind == PAINT_ANSWER_REFUSED && disabled.reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    Draw(*context, controller, mode, control, installedLabel);
    ExactEmpty(*context, query);
  }
  toolbox_host::failRegions = 2;
  Draw(*context, controller, mode, control, installedLabel);
  LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_REFUSED);
  toolbox_host::failRegions = 0;
  Draw(*context, controller, mode, control, installedLabel);
  ExactEmpty(*context, query);
  Access::unmount(scene);
  std::puts("Partial paint pins passed");
}
