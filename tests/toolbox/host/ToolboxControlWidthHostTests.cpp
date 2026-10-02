#include "app/layout/ColumnLayout.hpp"
#include "context/ToolboxTextContext.hpp"
#include "Script.h"
#include "app/layout/RowLayout.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/TestVerify.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include <cstdio>
#include <cstring>

void testToolboxRowSpacing(const char *mode);

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
          << Text(this->title.state()).TEST_ID("text")
          << (Text(this->title.state()).TEST_ID("wrapped") + BlockStyle().wrap(TEXT_WRAP_WORD))
          << (Text(this->title.state()).TEST_ID("ellipsis") + BlockStyle().truncation(TEXT_TRUNCATION_ELLIPSIS))
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
  if (std::strncmp(mode, "spacing-", 8) == 0)
  {
    testToolboxRowSpacing(mode);
    return 0;
  }
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
  if (std::strcmp(mode, "encoding-button") == 0)
  {
    const String label = String::Literal("Open\xE2\x80\xA6 \xC3\xA9");
    const std::string expected("Open\xC9 \x8E", 7);
    HostControl control = {0};
    std::string installed;
    toolbox_host::controlTitles.clear();
    LOKA_VERIFY(ReconcileToolboxButtonControl(&control, label, 0, installed));
    LOKA_VERIFY(toolbox_host::controlTitles.size() == 1);
    LOKA_VERIFY(toolbox_host::controlTitles[0] == expected);
    LOKA_VERIFY(installed == "Open\xE2\x80\xA6 \xC3\xA9");
    LOKA_VERIFY(ReconcileToolboxButtonControl(&control, label, 0, installed));
    LOKA_VERIFY(toolbox_host::controlTitles.size() == 1);
    { StateTrackerGuard guard(root->tracker()); root->title.set(label); }
    LayoutState offer = seat(0);
    const short measured = rowButton->context->layout(&controller, offer);
    Str255 encoded = {7, 'O', 'p', 'e', 'n', 0xC9, ' ', 0x8E};
    LOKA_VERIFY(measured == StringWidth(encoded) + 16);
  }
  else if (std::strcmp(mode, "encoding-popup") == 0)
  {
    String longLabel = String::Literal("");
    for (unsigned i = 0; i < 128; ++i)
      longLabel = String::Concat(longLabel, String::Literal("\xC3\xA9"));
    longLabel = String::Concat(longLabel, String::Literal(";X"));
    loka::Vector<String> items;
    items.push_back(longLabel);
    MutableState<int> selected(0);
    ToolboxPopupMenuContext &context = *static_cast<ToolboxPopupMenuContext *>(popup->context);
    context.updateData(&items, &selected, WriteSeat<int>(), 0, 0);
    Rect face; SetRect(&face, 10, 20, 250, 44);
    context.updateRect(face, 16);
    toolbox_host::reset();
    context.draw();
    const std::string expected = std::string(128, static_cast<char>(0x8E)) + ";X";
    LOKA_VERIFY(!toolbox_host::draws.empty());
    LOKA_VERIFY(toolbox_host::draws[0].bytes == expected);
    LOKA_VERIFY(toolbox_host::pascalDraws.size() == 1);
    LOKA_VERIFY(toolbox_host::pascalDraws[0] == expected);
    toolbox_host::menuAppends.clear(); toolbox_host::menuSets.clear();
    const Point click = {25, 15};
    LOKA_VERIFY(context.handleMouseDown(click, 0));
    LOKA_VERIFY(toolbox_host::menuAppends.size() == 1);
    LOKA_VERIFY(toolbox_host::menuAppends[0] == " ");
    LOKA_VERIFY(toolbox_host::menuSets.size() == 1);
    LOKA_VERIFY(toolbox_host::menuSets[0] == expected);
    LOKA_VERIFY(toolbox_host::disposedMenuItems.size() == 1);
    LOKA_VERIFY(toolbox_host::disposedMenuItems[0] == expected);
  }
  else if (std::strcmp(mode, "encoding-text") == 0)
  {
    const String label = String::Literal("Open\xE2\x80\xA6 \xC3\xA9");
    { StateTrackerGuard guard(root->tracker()); root->title.set(label); }
    const char *ids[] = {"text", "wrapped"};
    for (unsigned i = 0; i < 2; ++i)
    {
      Node *text = lookup(scene, ids[i]);
      text->setContext(new ToolboxTextContext(text->asTextNode(), &controller));
      LayoutState offer = seat(0);
      toolbox_host::reset();
      LOKA_VERIFY(text->context->layout(&controller, offer) == TextWidth("Open\xE2\x80\xA6 \xC3\xA9", 0, 10));
      text->context->render(&controller);
      LOKA_VERIFY(!toolbox_host::draws.empty());
      LOKA_VERIFY(toolbox_host::draws[0].bytes == "Open\xE2\x80\xA6 \xC3\xA9");
      LOKA_VERIFY(static_cast<ToolboxTextContext *>(text->context)->visibleWidth()
          == TextWidth("Open\xE2\x80\xA6 \xC3\xA9", 0, 10));
    }
    Node *ellipsis = lookup(scene, "ellipsis");
    ellipsis->setContext(new ToolboxTextContext(ellipsis->asTextNode(), &controller));
    LayoutState narrow = seat(32);
    toolbox_host::reset();
    ellipsis->context->layout(&controller, narrow);
    ellipsis->context->render(&controller);
    LOKA_VERIFY(toolbox_host::draws.size() == 1);
    LOKA_VERIFY(toolbox_host::draws[0].bytes == "Open...");
  }
  else if (std::strncmp(mode, "column-", 7) == 0)
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
