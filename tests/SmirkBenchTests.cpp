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

  loka::app::StackNode *findStack(SmirkBenchFixture &fixture, const char *testId)
  {
    loka::app::scene::Node *node =
        findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), testId);
    return node ? node->asStackNode() : 0;
  }

  void verifyLandscapeRowSeats(SmirkBenchFixture &fixture)
  {
    loka::app::StackNode *row = findStack(fixture, "SmirkBench.Panels");
    LOKA_VERIFY(row != 0);
    LOKA_VERIFY(row->props.axis_ == loka::app::STACK_AXIS_ROW);
    LOKA_VERIFY(row->childrenCount() == 2);
    loka::app::scene::Node *navSeat = row->childrenHead();
    loka::app::scene::Node *surface = navSeat ? navSeat->nextInComposition : 0;
    LOKA_VERIFY(surface != 0);
    LOKA_VERIFY(surface->nextInComposition == 0);

    loka::app::layout::RowWidthConsultation widths(row->childrenHead(), row->childrenCount(), 500, 4);
    const loka::app::layout::RowChildWidth nav = widths.next(navSeat);
    const loka::app::layout::RowChildWidth content = widths.next(surface);
    LOKA_VERIFY(nav.width() == 200);
    LOKA_VERIFY(nav.isLiveSeat());
    LOKA_VERIFY(content.width() == 296);
  }
} // namespace

void testSmirkBenchOrientationFlipsAxesAndRetainsSurface()
{
  SmirkBenchFixture fixture;
  loka::app::scene::Node *surface =
      findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.Surface");
  LOKA_VERIFY(surface != 0);
  LOKA_VERIFY(surface->asRectSurfaceNode() != 0);
  loka::app::StackNode *root = findStack(fixture, "SmirkBench.Panels");
  loka::app::StackNode *navPane = findStack(fixture, "SmirkBench.NavPane");
  LOKA_VERIFY(root != 0);
  LOKA_VERIFY(navPane != 0);
  LOKA_VERIFY(navPane->props.axis_ == loka::app::STACK_AXIS_COLUMN);
  verifyLandscapeRowSeats(fixture);

  // A portrait window flips both containers' axes. The Stack nodes and the
  // surface are retained (#555): only the axis props change. On the rails
  // the flip rides the resize that caused it, so this fixture asserts the
  // tree, and the seat pin below asserts the layout that follows.
  fixture.platform.skipNextProjectionForTesting();
  fixture.mainNode->refreshOrientationForTesting(loka::core::Frame(0, 0, 300, 500));
  flushSmirkBench(fixture.scene);
  LOKA_VERIFY(fixture.mainNode->orientationForTesting() == smirkbench::ORIENTATION_PORTRAIT);
  LOKA_VERIFY(findStack(fixture, "SmirkBench.Panels") == root);
  LOKA_VERIFY(findStack(fixture, "SmirkBench.NavPane") == navPane);
  LOKA_VERIFY(findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.Surface")
              == surface);
  LOKA_VERIFY(root->props.axis_ == loka::app::STACK_AXIS_COLUMN);
  LOKA_VERIFY(navPane->props.axis_ == loka::app::STACK_AXIS_ROW);
  loka::app::scene::Node *addButtonNode =
      findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.AddFace");
  LOKA_VERIFY(addButtonNode != 0);
  LOKA_VERIFY(addButtonNode->asButtonNode() != 0);

  // Back to landscape: same nodes, axes restored.
  fixture.platform.skipNextProjectionForTesting();
  fixture.mainNode->refreshOrientationForTesting(loka::core::Frame(0, 0, 500, 300));
  flushSmirkBench(fixture.scene);
  LOKA_VERIFY(findStack(fixture, "SmirkBench.Panels") == root);
  LOKA_VERIFY(findNodeByTestId(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene), "SmirkBench.Surface")
              == surface);
  LOKA_VERIFY(navPane->props.axis_ == loka::app::STACK_AXIS_COLUMN);
  verifyLandscapeRowSeats(fixture);
}

void testSmirkBenchOrientationDerivationAvoidsIdenticalWrites()
{
  SmirkBenchFixture fixture;
  fixture.mainNode->refreshOrientationForTesting(loka::core::Frame(0, 0, 300, 500));
  LOKA_VERIFY(fixture.mainNode->orientationForTesting() == smirkbench::ORIENTATION_PORTRAIT);
  LOKA_VERIFY(fixture.mainNode->consumeOrientationTrackerDirtForTesting());

  fixture.mainNode->refreshOrientationForTesting(loka::core::Frame(0, 0, 300, 500));
  LOKA_VERIFY(!fixture.mainNode->consumeOrientationTrackerDirtForTesting());

  // A square window is landscape; an unsized frame keeps the current value.
  fixture.mainNode->refreshOrientationForTesting(loka::core::Frame(0, 0, 400, 400));
  LOKA_VERIFY(fixture.mainNode->orientationForTesting() == smirkbench::ORIENTATION_LANDSCAPE);
  LOKA_VERIFY(fixture.mainNode->consumeOrientationTrackerDirtForTesting());

  fixture.mainNode->refreshOrientationForTesting(loka::core::Frame());
  LOKA_VERIFY(fixture.mainNode->orientationForTesting() == smirkbench::ORIENTATION_LANDSCAPE);
  LOKA_VERIFY(!fixture.mainNode->consumeOrientationTrackerDirtForTesting());
}

void testSmirkBenchSurfaceExtentTracksContentSeat()
{
  SmirkBenchFixture fixture(false);
  loka::app::RectSurfaceNode *surface = findSurface(fixture);
  LOKA_VERIFY(surface != 0);
  // Landscape 500x300: the surface fills the Row seat beside the 200 nav.
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

  // The same seat again delivers nothing.
  const loka::core::Frame unchangedExtent = fixture.mainNode->surfaceExtentForTesting();
  const unsigned long beforeSameProjection = fixture.platform.onChangeCallCount();
  fixture.platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene),
                                           layoutState(500, 300));
  LOKA_VERIFY(fixture.mainNode->surfaceExtentForTesting() == unchangedExtent);
  LOKA_VERIFY(fixture.platform.onChangeCallCount() == beforeSameProjection);

  // A wider landscape window widens the seat, and the faces follow.
  fixture.platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene),
                                           layoutState(520, 300));
  LOKA_VERIFY(fixture.mainNode->surfaceExtentForTesting() == loka::core::Frame(204, 0, 316, 300));
  fixture.platform.skipNextProjectionForTesting();
  const bool widerFaceAdded =
      fixture.model.addFaceForTesting(loka::app::RectSprite(30000, 30000, 24, 24), 1, 1);
  LOKA_VERIFY(widerFaceAdded);
  LOKA_VERIFY(fixture.model.faceForTesting(1).x + fixture.model.faceForTesting(1).width == 316);
  LOKA_VERIFY(fixture.model.faceForTesting(1).y + fixture.model.faceForTesting(1).height == 300);

  // Portrait 300x500: the nav row (Button height + spacing = 44) sits above,
  // the surface takes the Column's remainder; the seat delivery reclamps.
  fixture.platform.skipNextProjectionForTesting();
  fixture.mainNode->refreshOrientationForTesting(loka::core::Frame(0, 0, 300, 500));
  drainSmirkBench(fixture.scene);
  fixture.platform.skipNextProjectionForTesting();
  fixture.platform.projectLayoutForTesting(loka::dsl::testing::SceneTestAccess::rootNode(fixture.scene),
                                           layoutState(300, 500));
  drainSmirkBench(fixture.scene);
  LOKA_VERIFY(findSurface(fixture) == surface);
  LOKA_VERIFY(fixture.mainNode->surfaceExtentForTesting() == loka::core::Frame(0, 44, 300, 456));
  LOKA_VERIFY(fixture.model.faceForTesting(0).x + fixture.model.faceForTesting(0).width <= 300);
  LOKA_VERIFY(fixture.model.faceForTesting(1).x + fixture.model.faceForTesting(1).width <= 300);
  fixture.platform.skipNextProjectionForTesting();
  const bool portraitFaceAdded =
      fixture.model.addFaceForTesting(loka::app::RectSprite(30000, 30000, 24, 24), 1, 1);
  LOKA_VERIFY(portraitFaceAdded);
  LOKA_VERIFY(fixture.model.faceForTesting(2).x + fixture.model.faceForTesting(2).width == 300);
  LOKA_VERIFY(fixture.model.faceForTesting(2).y + fixture.model.faceForTesting(2).height == 456);
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
