#include "RibbonTests.hpp"
#include "app/nodes/controls/Ribbon.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::dsl;
  using namespace loka::dsl::testing;

  class RibbonGeometryContext : public NodeContext
  {
  public:
    LayoutState rect;
    virtual short layout(IPlatformController *, LayoutState &state)
    {
      this->rect = state;
      return state.width;
    }
  };

  class RibbonGeometryHandler : public IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return NodeTypeToken<ButtonNode>();
    }
    virtual NodeContext *ensureContext(Node *node, IPlatformController *, const LayoutState &)
    {
      if (!node->getContext())
        node->setContext(new RibbonGeometryContext());
      return node->getContext();
    }
  };

  void countRibbonClick(void *data)
  {
    ++*static_cast<int *>(data);
  }

  ButtonNode *ribbonButton(Scene &scene, const char *anchor, int index)
  {
    ButtonNode *node = 0;
    FlowError error;
    LOKA_VERIFY(ResolveSelector(&scene, WithinAnchor(anchor).descendant<ButtonNode>(index + 1), node, error)
                == FLOW_STEP_SUCCEEDED);
    LOKA_VERIFY(node != 0);
    return node;
  }
} // namespace

void testRibbonFixedSeatsAndClicks()
{
  RibbonGeometryHandler handler;
  NullScenePlatformController platform;
  LOKA_VERIFY(platform.registerNodeHandler(&handler));
  loka::core::EmitterState events[4];
  int clicks[4] = {0, 0, 0, 0};
  for (int i = 0; i < 4; ++i)
    events[i].bind(&countRibbonClick, &clicks[i], false);
  NodeDefinitionBase *definition =
      (Column() << (RibbonControl().testId("RibbonUnderTest")
                    << RibbonItem("New").onClick(&events[0]) << RibbonItem("Open...").onClick(&events[1])
                    << RibbonItem("Save").onClick(&events[2])
                    << RibbonItem("Save As...").onClick(&events[3]).width(120))
                << (Row().testId("GapMetric")
                    << (Box().size(10, 0) << Button("A")) << (Box().size(10, 0) << Button("B"))))
          .clone();
  LOKA_VERIFY(definition != 0);
  Scene scene(definition);
  LOKA_VERIFY(scene.mount(&platform));
  SceneTestAccess::updateAttached(scene, true);
  // As in the responsive layout fixtures, record the projected Button seats.
  RibbonGeometryContext *rects[6];
  for (int i = 0; i < 6; ++i)
  {
    ButtonNode *node = ribbonButton(scene, i < 4 ? "RibbonUnderTest" : "GapMetric", i < 4 ? i : i - 4);
    rects[i] = static_cast<RibbonGeometryContext *>(node->getContext());
    LOKA_VERIFY(rects[i] != 0);
  }
  LayoutState viewport;
  viewport.width = 640;
  viewport.height = 160;
  viewport.lineHeight = 20;
  platform.projectLayoutForTesting(SceneTestAccess::rootNode(scene), viewport);
  // The Null rail keeps its Row metric private. A normal Row with known fixed
  // seats measures that same metric without duplicating its numeric value.
  const int gap = rects[5]->rect.x - (rects[4]->rect.x + rects[4]->rect.width);
  LOKA_VERIFY(gap > 0);
  const short widths[] = {80, 80, 80, 120};
  const char *titles[] = {"New", "Open...", "Save", "Save As..."};
  for (int i = 0; i < 4; ++i)
  {
    const LayoutState &rect = rects[i]->rect;
    std::printf("Ribbon item %d: x=%d width=%d expected=%d gap=%d\n", i, rect.x, rect.width, widths[i], gap);
    std::fflush(stdout);
    LOKA_VERIFY(rect.width == widths[i]);
    if (i)
    {
      LOKA_VERIFY(rect.x > rects[i - 1]->rect.x);
      LOKA_VERIFY(rect.x - (rects[i - 1]->rect.x + rects[i - 1]->rect.width) == gap);
    }
    LOKA_VERIFY(
        ribbonButton(scene, "RibbonUnderTest", i)->props.text_->get().equals(loka::core::String::Literal(titles[i])));
  }
  // The Column offers its full width to the ribbon Row.
  LOKA_VERIFY(rects[3]->rect.x + rects[3]->rect.width < viewport.x + viewport.width);
  for (int i = 0; i < 4; ++i)
  {
    Scene *out = 0;
    FlowError error;
    LOKA_VERIFY(ClickButton(WithinAnchor("RibbonUnderTest").descendant<ButtonNode>(i + 1)).run(&scene, out, error)
                == FLOW_STEP_SUCCEEDED);
    for (int j = 0; j < 4; ++j)
      LOKA_VERIFY(clicks[j] == (j <= i ? 1 : 0));
  }
  SceneTestAccess::unmount(scene);
  for (int i = 0; i < 4; ++i)
    events[i].unbind(&countRibbonClick, &clicks[i]);
}
