#include "SmirkBenchTests.hpp"

#include "../example/SmirkBench/src/MainNode.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  loka::app::scene::Node *findNodeByTestId(loka::app::scene::Node *node, const char *testId)
  {
    if (!node)
    {
      return 0;
    }
    if (node->testId() == testId)
    {
      return node;
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0; child;
         child = child->nextInComposition)
    {
      loka::app::scene::Node *match = findNodeByTestId(child, testId);
      if (match)
      {
        return match;
      }
    }
    return 0;
  }

  void flushSmirkBench(loka::app::scene::Scene &scene)
  {
    if (scene.hasPendingInvalidation())
    {
      LOKA_VERIFY(scene.flushInvalidation());
    }
  }

  void drainSmirkBench(loka::app::scene::Scene &scene)
  {
    for (int attempt = 0; attempt < 4 && scene.hasPendingInvalidation(); ++attempt)
    {
      (void)scene.flushInvalidation();
    }
    assert(!scene.hasPendingInvalidation());
  }

  struct SmirkBenchFixture
  {
    explicit SmirkBenchFixture(bool addInitialFace = true)
        : model(640, 400, addInitialFace),
          platform(),
          scene(loka::app::scene::Boundary<smirkbench::MainNode>(smirkbench::MainProps(&this->model))),
          mainNode(0)
    {
      this->scene.mount(&this->platform);
      this->scene.updateAttached(true);
      this->mainNode =
          static_cast<smirkbench::MainNode *>(loka::dsl::testing::SceneTestAccess::rootBoundary(this->scene));
      LOKA_VERIFY(this->mainNode != 0);
    }

    ~SmirkBenchFixture()
    {
      this->scene.unmount();
    }

    smirkbench::SmirkModel model;
    NullScenePlatformController platform;
    loka::app::scene::Scene scene;
    smirkbench::MainNode *mainNode;
  };

  loka::app::RectSurfaceNode *findSurface(SmirkBenchFixture &fixture)
  {
    loka::app::scene::Node *node =
        findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.Surface");
    return node ? node->asRectSurfaceNode() : 0;
  }

  loka::app::scene::LayoutState layoutState(short width, short height)
  {
    loka::app::scene::LayoutState state;
    state.width = width;
    state.height = height;
    return state;
  }

  void verifyRowSeats(SmirkBenchFixture &fixture, int expectedNavWidth, int expectedSurfaceX, int expectedSurfaceWidth)
  {
    loka::app::scene::Node *rowNode =
        findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.Root");
    LOKA_VERIFY(rowNode != 0);
    loka::app::StackNode *row = rowNode->asStackNode();
    LOKA_VERIFY(row != 0);
    LOKA_VERIFY(row->childrenCount() == 2);
    loka::app::scene::Node *navSeat = row->childrenHead();
    loka::app::scene::Node *surfaceColumn = navSeat ? navSeat->nextInComposition : 0;
    LOKA_VERIFY(surfaceColumn != 0);
    LOKA_VERIFY(surfaceColumn->nextInComposition == 0);

    loka::app::layout::RowWidthConsultation widths(row->childrenHead(), row->childrenCount(), 500, 4);
    const loka::app::layout::RowChildWidth nav = widths.next(navSeat);
    const loka::app::layout::RowChildWidth content = widths.next(surfaceColumn);
    LOKA_VERIFY(nav.width() == expectedNavWidth);
    LOKA_VERIFY(content.width() == expectedSurfaceWidth);
    const int surfaceX = nav.isLiveSeat() ? nav.width() + 4 : 0;
    LOKA_VERIFY(surfaceX == expectedSurfaceX);
  }
} // namespace

void testSmirkBenchNavModesResizeSeatsAndRetainSurface()
{
  SmirkBenchFixture fixture;
  loka::app::scene::Node *surface =
      findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.Surface");
  LOKA_VERIFY(surface != 0);
  LOKA_VERIFY(surface->asRectSurfaceNode() != 0);
  verifyRowSeats(fixture, 200, 204, 296);

  // The nav-mode change itself is one recompose. Projection is held back so
  // the rail's seat delivery (the second change below) is observed on its own.
  const unsigned long beforeClosed = fixture.platform.onChangeCallCount();
  fixture.platform.skipNextProjectionForTesting();
  fixture.mainNode->setNavModeForTesting(smirkbench::NAV_NARROW_CLOSED);
  flushSmirkBench(fixture.scene);
  LOKA_VERIFY(fixture.platform.onChangeCallCount() == beforeClosed + 1);
  LOKA_VERIFY((fixture.platform.lastOnChangeFlags() & loka::app::scene::NODE_DIRTY_CHILD) != 0);
  LOKA_VERIFY((fixture.platform.lastOnChangeFlags() & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

  // Laying the narrow tree out delivers the surface's new seat after the
  // pass (below the toggle Button, the nav gone); the model moves its walls
  // there and republishes the surface, which is the one further change.
  const unsigned long beforeSeat = fixture.platform.onChangeCallCount();
  fixture.platform.skipNextProjectionForTesting();
  fixture.platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene),
                                           layoutState(479, 300));
  flushSmirkBench(fixture.scene);
  LOKA_VERIFY(fixture.mainNode->surfaceExtentForTesting() == loka::core::Frame(0, 44, 479, 256));
  LOKA_VERIFY(fixture.platform.onChangeCallCount() == beforeSeat + 1);
  LOKA_VERIFY((fixture.platform.lastOnChangeFlags() & loka::app::scene::NODE_DIRTY_PROPS) != 0);
  LOKA_VERIFY(findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.Surface")
              == surface);
  verifyRowSeats(fixture, 0, 0, 500);
  loka::app::scene::Node *menuButtonNode =
      findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.NavToggle");
  LOKA_VERIFY(menuButtonNode != 0);
  loka::app::ButtonNode *menuButton = menuButtonNode->asButtonNode();
  LOKA_VERIFY(menuButton != 0);
  LOKA_VERIFY(menuButton->props.text_ != 0);
  LOKA_VERIFY(menuButton->props.text_->get().equals(loka::core::String::Literal("=")));

  fixture.mainNode->setNavModeForTesting(smirkbench::NAV_NARROW_OPEN);
  flushSmirkBench(fixture.scene);
  LOKA_VERIFY(findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.Surface")
              == surface);
  verifyRowSeats(fixture, 200, 204, 296);
}

void testSmirkBenchNavModeDerivationAvoidsIdenticalWrites()
{
  SmirkBenchFixture fixture;
  fixture.mainNode->refreshNavModeForTesting(loka::core::Frame(0, 0, 479, 300));
  LOKA_VERIFY(fixture.mainNode->navModeForTesting() == smirkbench::NAV_NARROW_CLOSED);
  LOKA_VERIFY(fixture.mainNode->consumeNavModeTrackerDirtForTesting());

  fixture.mainNode->refreshNavModeForTesting(loka::core::Frame(0, 0, 479, 300));
  LOKA_VERIFY(!fixture.mainNode->consumeNavModeTrackerDirtForTesting());

  fixture.mainNode->setNavOpenForTesting(true);
  fixture.mainNode->refreshNavModeForTesting(loka::core::Frame(0, 0, 479, 300));
  LOKA_VERIFY(fixture.mainNode->navModeForTesting() == smirkbench::NAV_NARROW_OPEN);
  LOKA_VERIFY(fixture.mainNode->consumeNavModeTrackerDirtForTesting());

  fixture.mainNode->refreshNavModeForTesting(loka::core::Frame(0, 0, 480, 300));
  LOKA_VERIFY(fixture.mainNode->navModeForTesting() == smirkbench::NAV_WIDE);
  LOKA_VERIFY(fixture.mainNode->consumeNavModeTrackerDirtForTesting());
}

void testSmirkBenchSurfaceExtentTracksContentSeat()
{
  {
    smirkbench::SmirkModel beforeLayout(640, 400, false);
    const bool faceAdded =
        beforeLayout.addFaceForTesting(loka::app::RectSprite(30000, 30000, 24, 24), 1, 1);
    LOKA_VERIFY(faceAdded);
    const loka::app::RectSprite &face = beforeLayout.faceForTesting(0);
    LOKA_VERIFY(face.x + face.width == 640);
    LOKA_VERIFY(face.y + face.height == 400);
  }

  SmirkBenchFixture fixture(false);
  loka::app::RectSurfaceNode *surface = findSurface(fixture);
  LOKA_VERIFY(surface != 0);

  fixture.mainNode->refreshNavModeForTesting(loka::core::Frame(0, 0, 500, 300));
  drainSmirkBench(fixture.scene);
  fixture.platform.skipNextProjectionForTesting();
  fixture.platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene),
                                           layoutState(500, 300));
  LOKA_VERIFY(findSurface(fixture) == surface);
  LOKA_VERIFY(fixture.mainNode->surfaceExtentForTesting() == loka::core::Frame(204, 0, 296, 300));
  fixture.platform.skipNextProjectionForTesting();
  const bool wideFaceAdded =
      fixture.model.addFaceForTesting(loka::app::RectSprite(30000, 30000, 24, 24), 1, 1);
  LOKA_VERIFY(wideFaceAdded);
  LOKA_VERIFY(fixture.model.faceForTesting(0).x + fixture.model.faceForTesting(0).width == 296);
  LOKA_VERIFY(fixture.model.faceForTesting(0).y + fixture.model.faceForTesting(0).height == 300);

  const loka::core::Frame unchangedExtent = fixture.mainNode->surfaceExtentForTesting();
  const unsigned long beforeSameProjection = fixture.platform.onChangeCallCount();
  fixture.platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene),
                                           layoutState(500, 300));
  LOKA_VERIFY(fixture.mainNode->surfaceExtentForTesting() == unchangedExtent);
  LOKA_VERIFY(fixture.platform.onChangeCallCount() == beforeSameProjection);

  fixture.mainNode->refreshNavModeForTesting(loka::core::Frame(0, 0, 520, 300));
  drainSmirkBench(fixture.scene);
  fixture.platform.skipNextProjectionForTesting();
  fixture.platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene),
                                           layoutState(520, 300));
  LOKA_VERIFY(findSurface(fixture) == surface);
  LOKA_VERIFY(fixture.mainNode->surfaceExtentForTesting() == loka::core::Frame(204, 0, 316, 300));
  fixture.platform.skipNextProjectionForTesting();
  const bool widerFaceAdded =
      fixture.model.addFaceForTesting(loka::app::RectSprite(30000, 30000, 24, 24), 1, 1);
  LOKA_VERIFY(widerFaceAdded);
  LOKA_VERIFY(fixture.model.faceForTesting(1).x + fixture.model.faceForTesting(1).width == 316);
  LOKA_VERIFY(fixture.model.faceForTesting(1).y + fixture.model.faceForTesting(1).height == 300);

  fixture.mainNode->refreshNavModeForTesting(loka::core::Frame(0, 0, 479, 300));
  fixture.platform.skipNextProjectionForTesting();
  drainSmirkBench(fixture.scene);
  fixture.platform.skipNextProjectionForTesting();
  fixture.platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene),
                                           layoutState(479, 300));
  LOKA_VERIFY(findSurface(fixture) == surface);
  LOKA_VERIFY(fixture.mainNode->surfaceExtentForTesting() == loka::core::Frame(0, 44, 479, 256));
  LOKA_VERIFY(fixture.model.faceForTesting(0).y + fixture.model.faceForTesting(0).height == 256);
  LOKA_VERIFY(fixture.model.faceForTesting(1).y + fixture.model.faceForTesting(1).height == 256);
  fixture.platform.skipNextProjectionForTesting();
  const bool narrowFaceAdded =
      fixture.model.addFaceForTesting(loka::app::RectSprite(30000, 30000, 24, 24), 1, 1);
  LOKA_VERIFY(narrowFaceAdded);
  LOKA_VERIFY(fixture.model.faceForTesting(2).x + fixture.model.faceForTesting(2).width == 479);
  LOKA_VERIFY(fixture.model.faceForTesting(2).y + fixture.model.faceForTesting(2).height == 256);
}

void testSmirkModelReflectsRefusesAndReclamps()
{
  smirkbench::SmirkModel model(100, 80, false);
  LOKA_VERIFY(model.addFaceForTesting(loka::app::RectSprite(76, 10, 24, 24), 5, 2));
  model.advanceFrame(smirkbench::kFixedStepSeconds);
  LOKA_VERIFY(model.faceForTesting(0).x <= 76);
  LOKA_VERIFY(model.faceForTesting(0).x >= 0);
  LOKA_VERIFY(model.faceForTesting(0).y >= 0);
  LOKA_VERIFY(model.faceForTesting(0).y <= 56);
  LOKA_VERIFY(model.velocityXForTesting(0) < 0);

  smirkbench::SmirkModel cappedModel(100, 80, false);
  for (int i = 0; i < loka::app::RectSurfaceModel::kMaxRects; ++i)
  {
    LOKA_VERIFY(cappedModel.addFace());
  }
  LOKA_VERIFY(!cappedModel.addFace());
  LOKA_VERIFY(cappedModel.faceCount() == loka::app::RectSurfaceModel::kMaxRects);

  model.updateBounds(30, 30);
  for (int i = 0; i < model.faceCount(); ++i)
  {
    const loka::app::RectSprite &face = model.faceForTesting(i);
    LOKA_VERIFY(face.x >= 0);
    LOKA_VERIFY(face.y >= 0);
    LOKA_VERIFY(face.x + face.width <= 30);
    LOKA_VERIFY(face.y + face.height <= 30);
  }

  SmirkBenchFixture fixture;
  fixture.mainNode->setNavModeForTesting(smirkbench::NAV_NARROW_OPEN);
  flushSmirkBench(fixture.scene);
  loka::app::scene::Node *addButtonNode =
      findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.AddFace");
  LOKA_VERIFY(addButtonNode != 0);
  loka::app::ButtonNode *addButton = addButtonNode->asButtonNode();
  LOKA_VERIFY(addButton != 0);
  LOKA_VERIFY(addButton->props.onClick_ != 0);
  while (fixture.model.faceCount() < loka::app::RectSurfaceModel::kMaxRects)
  {
    addButton->props.onClick_->emit();
  }
  LOKA_VERIFY(addButton->props.enabled_ != 0);
  LOKA_VERIFY(!addButton->props.enabled_->get());
}
