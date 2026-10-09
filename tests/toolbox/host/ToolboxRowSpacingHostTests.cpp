#include "ToolboxLayoutMetrics.hpp"
#include "ToolboxPlatformLayoutHandlers.hpp"
#include "app/nodes/controls/Ribbon.hpp"
#include "context/ToolboxTextContext.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>
#include <cstring>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;

  class SpacingNode;
  typedef BoundaryPropsFor<SpacingNode> SpacingProps;
  class SpacingNode : public StdCompositionBoundaryNodeBase<SpacingProps>
  {
  public:
    typedef SpacingProps::TypeTag TypeTag;
    explicit SpacingNode(const SpacingProps &props) : StdCompositionBoundaryNodeBase<SpacingProps>(props) {}
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Column() << (Column().TEST_ID("unaligned")
                            << (Row() << Button("A").TEST_ID("row-a") << Button("B").TEST_ID("row-b"))
                            << Text("below").TEST_ID("row-below"))
                         << (Column().TEST_ID("aligned")
                             << (Row().alignVertical(VERTICAL_ALIGNMENT_CENTER)
                                 << Button("A").TEST_ID("aligned-a") << Button("B").TEST_ID("aligned-b"))
                             << Text("below").TEST_ID("aligned-below"))
                         << (Column().TEST_ID("plain") << Button("A").TEST_ID("plain-a")
                                                     << Button("B").TEST_ID("plain-b"))
                         << (RibbonControl().testId("ribbon")
                             << RibbonItem("New") << RibbonItem("Save As...").width(120)));
    }
  };

  Node *SpacingLookup(Scene &scene, const char *id)
  {
    Node *node = 0;
    loka::dsl::FlowError error;
    LOKA_VERIFY(loka::dsl::testing::LookupNodeById<Node>(&scene, id, node, error)
                == loka::dsl::FLOW_STEP_SUCCEEDED);
    LOKA_VERIFY(node != 0);
    return node;
  }

  class SpacingTraversal : public IPlatformLayoutTraversal
  {
  public:
    explicit SpacingTraversal(ToolboxScenePlatformController &controller)
        : controller_(controller), resultY_(0), paintedCount_(0)
    {
      RegisterToolboxPlatformLayoutHandlers(this->registry_);
    }
    virtual bool queryNaturalWidth(Node *child, short &width) const
    {
      return this->controller_.queryNaturalWidth(child, width);
    }
    virtual int layoutChild(Node *node, const LayoutState &offer)
    {
      LayoutState state = offer;
      short width = 0;
      if (!ApplyToolboxPlatformLayoutHandler(this->registry_, *node, state, *this, width))
      {
        if (node->asButtonNode())
          node->setContext(new ToolboxButtonContext(node->asButtonNode(), &this->controller_));
        else
        {
          LOKA_VERIFY(node->asTextNode() != 0);
          node->setContext(new ToolboxTextContext(node->asTextNode(), &this->controller_));
        }
        width = node->context->layout(&this->controller_, state);
        node->context->render(&this->controller_);
        LOKA_VERIFY(this->paintedCount_ < 3);
        if (node->asButtonNode())
          this->painted_[this->paintedCount_] = toolbox_host::controlRect;
        else
        {
          const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
          const PaintAnswer answer = static_cast<ToolboxTextContext *>(node->context)->queryPaintDamage(query);
          LOKA_VERIFY(answer.kind == PAINT_ANSWER_EXACT);
          SetRect(&this->painted_[this->paintedCount_], answer.damage.x, answer.damage.y,
                  answer.damage.x + answer.damage.width, answer.damage.y + answer.damage.height);
        }
        ++this->paintedCount_;
      }
      this->resultY_ = state.y;
      return width;
    }
    virtual void setLayoutResultY(short y) { this->resultY_ = y; }
    virtual short layoutResultY() const { return this->resultY_; }
    const Rect &painted(unsigned i) const
    {
      LOKA_VERIFY(i < this->paintedCount_);
      return this->painted_[i];
    }

  private:
    ToolboxScenePlatformController &controller_;
    PlatformLayoutHandlerRegistry registry_;
    short resultY_;
    Rect painted_[3];
    unsigned paintedCount_;
  };
}

void testToolboxRowSpacing(const char *mode)
{
  const bool ribbon = std::strcmp(mode, "spacing-ribbon") == 0;
  const bool plain = std::strcmp(mode, "spacing-column") == 0;
  const bool aligned = std::strcmp(mode, "spacing-aligned") == 0;
  ToolboxWindow window;
  ToolboxScenePlatformController controller(&window);
  Scene scene((Boundary<SpacingNode>(SpacingProps())));
  scene.mount(&controller);
  typedef loka::dsl::testing::SceneTestAccess Access;
  Access::updateAttached(scene, true);
  SpacingTraversal traversal(controller);
  LayoutState state;
  state.x = 10;
  state.y = 20;
  state.width = 240;
  state.height = 100;
  state.lineHeight = 16;
  state.spacing = 6;
  traversal.layoutChild(SpacingLookup(scene, ribbon ? "ribbon" : plain ? "plain" : aligned ? "aligned" : "unaligned"), state);
  const Rect a = traversal.painted(0), b = traversal.painted(1);
  if (ribbon)
  {
    std::printf("Ribbon Toolbox widths: %d, %d; gap: %d\n", a.right - a.left, b.right - b.left, b.left - a.right);
    std::fflush(stdout);
    const short natural = controller.measurePushButtonNaturalWidth(loka::core::String::Literal("New"));
    LOKA_VERIFY(a.right - a.left == natural);
    // Standalone natural layout uses the same formula as the controller answer.
    loka::dsl::FlowError error;
    ButtonNode *button = 0;
    LOKA_VERIFY(loka::dsl::testing::ResolveSelector(&scene,
        loka::dsl::testing::WithinAnchor("ribbon").descendant<ButtonNode>(1), button, error)
        == loka::dsl::FLOW_STEP_SUCCEEDED);
    short first = 0, second = 0;
    LOKA_VERIFY(controller.queryNaturalWidth(button, first));
    LOKA_VERIFY(controller.queryNaturalWidth(button, second));
    LOKA_VERIFY(first == natural && second == natural);
    LayoutState standalone = state;
    standalone.width = 0;
    LOKA_VERIFY(button->context->layout(&controller, standalone) == natural);
    LOKA_VERIFY(b.right - b.left == 120);
    LOKA_VERIFY(b.left > a.left);
    LOKA_VERIFY(b.left - a.right == state.spacing);
    Access::unmount(scene);
    return;
  }
  const Rect next = traversal.painted(plain ? 1 : 2);
  const short bottom = plain ? a.bottom : (a.bottom > b.bottom ? a.bottom : b.bottom);
  const short gap = static_cast<short>(next.top - bottom);
  std::printf("%s: A=(%d,%d,%d,%d) B=(%d,%d,%d,%d) next-top=%d gap=%d\n", mode,
              a.left, a.top, a.right, a.bottom, b.left, b.top, b.right, b.bottom, next.top, gap);
  std::fflush(stdout);
  LOKA_VERIFY(a.top == 20 && a.bottom == 40);
  LOKA_VERIFY(b.bottom - b.top == 20);
  if (!plain)
  {
    LOKA_VERIFY(b.top == a.top && b.bottom == a.bottom);
    LOKA_VERIFY(b.left - a.right == 6);
  }
  LOKA_VERIFY(gap == 6);
  Access::unmount(scene);
}
