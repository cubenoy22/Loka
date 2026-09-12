#include "PaintBaselineTests.hpp"

#include "../example/HelloWorld/src/MainNode.hpp"
#include "../example/ScrapbookUI/src/MainNode.hpp"
#include "../example/MineSweeper/src/MainNode.hpp"
#include "../example/SimpleViewer/src/MainNode.hpp"
#include "../example/FloppyBird/src/MainNode.hpp"
#include "../example/SmirkBench/src/MainNode.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using loka::dsl::testing::SceneTestAccess;

  unsigned checkHints(Node *node)
  {
    if (!node)
      return 0;
    unsigned count = 0;
    if (BoundaryNode *boundary = node->asBoundary())
    {
      const BoundaryUpdateResult &result = boundary->updateResult();
      LOKA_VERIFY(!result.requiresCompositedPaint());
      LOKA_VERIFY(!result.hasOpaqueCoverageHint());
      ++count;
    }
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
      count += checkHints(child);
    return count;
  }

  /** Observes completed results synchronously before apply clears them. */
  class BaselinePlatform : public NullScenePlatformController
  {
  public:
    enum Observation
    {
      OBSERVE_APPLIES,
      OBSERVE_BOX_PAINT,
      OBSERVE_EXAMPLE_HINTS,
      ABSENT_SURFACE_BOUNDS,
      LAYOUT_SURFACE_BOUNDS,
      PAINT_SURFACE_BOUNDS
    };
    BaselinePlatform()
        : observation(OBSERVE_APPLIES),
          hintChecks(0)
    {
    }

    virtual void onChange(Node *root, NodeDirtyFlags flags, bool rebuild)
    {
      if (this->observation == OBSERVE_BOX_PAINT)
      {
        LOKA_VERIFY(root && root->asBoundary());
        LOKA_VERIFY(root->asBoundary()->updateResult().paint.hasPaintWork);
      }
      if (this->observation == OBSERVE_EXAMPLE_HINTS)
        this->hintChecks += checkHints(root);
      NullScenePlatformController::onChange(root, flags, rebuild);
    }

    virtual void onBoundaryApply(Node *root,
                                 BoundaryNode *boundary,
                                 const BoundaryLocalApplyInfo &info,
                                 const PlatformApplyPlan &plan)
    {
      this->applied.push_back(boundary);
      LOKA_VERIFY(plan.hasLocalPaintWork(boundary));
      for (size_t i = 0; i < this->foreignRoots.size(); ++i)
        if (this->foreignRoots[i] != boundary)
          LOKA_VERIFY(!plan.hasLocalPaintWork(this->foreignRoots[i]));
      if (this->observation == ABSENT_SURFACE_BOUNDS || this->observation == LAYOUT_SURFACE_BOUNDS
          || this->observation == PAINT_SURFACE_BOUNDS)
      {
        LOKA_VERIFY(info.paintKind == LOCAL_APPLY_PAINT_GENERIC);
        LOKA_VERIFY(!info.paintIsOpaque);
        assert(!info.hasCompositedPaintWork());
        if (this->observation == ABSENT_SURFACE_BOUNDS)
        {
          LOKA_VERIFY(info.boundsKind == LOCAL_APPLY_BOUNDS_NONE);
          LOKA_VERIFY(info.bounds == 0);
          LOKA_VERIFY(!info.hasPaintSpecificBoundsHint);
        }
        else
        {
          const bool paintSpecific = this->observation == PAINT_SURFACE_BOUNDS;
          LOKA_VERIFY(info.boundsKind == (paintSpecific ? LOCAL_APPLY_BOUNDS_PAINT : LOCAL_APPLY_BOUNDS_LAYOUT));
          LOKA_VERIFY(info.hasPaintSpecificBoundsHint == paintSpecific);
          LOKA_VERIFY(info.bounds != 0);
          const BoundaryNode::LayoutBounds &bounds = boundary->layoutBounds();
          LOKA_VERIFY(bounds.valid && bounds.width > 0 && bounds.height > 0);
          LOKA_VERIFY(info.bounds->x == bounds.x && info.bounds->y == bounds.y);
          LOKA_VERIFY(info.bounds->width == bounds.width && info.bounds->height == bounds.height);
        }
      }
      if (this->observation == OBSERVE_BOX_PAINT)
        LOKA_VERIFY(boundary->updateResult().paint.hasPaintWork);
      if (this->observation == OBSERVE_EXAMPLE_HINTS)
        this->hintChecks += checkHints(boundary);
      NullScenePlatformController::onBoundaryApply(root, boundary, info, plan);
    }

    Observation observation;
    unsigned hintChecks;
    std::vector<BoundaryNode *> applied;
    std::vector<BoundaryNode *> foreignRoots;
  };

  void settle(Scene &scene)
  {
    for (int i = 0; scene.hasPendingInvalidation() && i < 8; ++i)
      LOKA_VERIFY(scene.flushInvalidation());
    assert(!scene.hasPendingInvalidation());
  }

  class BoxBoundary;
  typedef BoundaryPropsFor<BoxBoundary> BoxProps;
  class BoxBoundary : public BoundaryNodeFor<BoxBoundary>
  {
  public:
    explicit BoxBoundary(const BoxProps &props)
        : BoundaryNodeFor<BoxBoundary>(props)
    {
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Box());
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
  };

  class PairBoundary;
  typedef BoundaryPropsFor<PairBoundary> PairProps;
  class PairBoundary : public BoundaryNodeFor<PairBoundary>
  {
  public:
    explicit PairBoundary(const PairProps &props)
        : BoundaryNodeFor<PairBoundary>(props)
    {
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(VStack() << Boundary<BoxBoundary>() << Boundary<BoxBoundary>());
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
  };

  BoundaryNode *firstChildBoundary(BoundaryNode *root)
  {
    Node *stack = root->childrenHead();
    assert(stack && stack->asNestable());
    Node *child = stack->asNestable()->childrenHead();
    LOKA_VERIFY(child && child->asBoundary());
    return child->asBoundary();
  }

  void checkCompression(NodeDirtyFlags parentFlags, unsigned expected)
  {
    BaselinePlatform platform;
    Scene scene((Boundary<PairBoundary>()));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    settle(scene);
    BoundaryNode *parent = SceneTestAccess::rootBoundary(scene);
    BoundaryNode *child = firstChildBoundary(parent);
    platform.applied.clear();
    const loka::app::scene::testing::PaintBaselineStats before = loka::app::scene::testing::paintBaselineStats();
    parent->markViewDirty(parentFlags);
    child->markViewDirty(NODE_DIRTY_PROPS);
    LOKA_VERIFY(scene.flushInvalidation());
    assert(!scene.hasPendingInvalidation());
    LOKA_VERIFY(platform.applied.size() == expected);
    LOKA_VERIFY(platform.applied[0] == parent);
    if (expected == 2)
      LOKA_VERIFY(platform.applied[1] == child);
    const loka::app::scene::testing::PaintBaselineStats after = loka::app::scene::testing::paintBaselineStats();
    LOKA_VERIFY(after.boundaryUpdateVisits - before.boundaryUpdateVisits == expected);
    LOKA_VERIFY(after.boundaryApplyCallbacks - before.boundaryApplyCallbacks == expected);
    LOKA_VERIFY(after.dirtySourceDeclarations > before.dirtySourceDeclarations);
    std::printf("paint baseline compression: flags=%d visits=%lu declarations=%lu callbacks=%lu\n",
                static_cast<int>(parentFlags),
                after.boundaryUpdateVisits - before.boundaryUpdateVisits,
                after.dirtySourceDeclarations - before.dirtySourceDeclarations,
                after.boundaryApplyCallbacks - before.boundaryApplyCallbacks);
    loka::dsl::testing::SceneTestAccess::unmount(scene);
  }

  template <class Definition> void checkExample(const char *name, const Definition &definition)
  {
    BaselinePlatform platform;
    platform.observation = BaselinePlatform::OBSERVE_EXAMPLE_HINTS;
    Scene scene(definition);
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    settle(scene);
    LOKA_VERIFY(platform.hintChecks > 0);
    const unsigned mountChecks = platform.hintChecks;
    const loka::app::scene::testing::PaintBaselineStats before = loka::app::scene::testing::paintBaselineStats();
    scene.requestInvalidate(NODE_DIRTY_PROPS);
    LOKA_VERIFY(scene.flushInvalidation());
    LOKA_VERIFY(platform.hintChecks > mountChecks);
    const loka::app::scene::testing::PaintBaselineStats after = loka::app::scene::testing::paintBaselineStats();
    LOKA_VERIFY(after.boundaryUpdateVisits > before.boundaryUpdateVisits);
    LOKA_VERIFY(after.boundaryApplyCallbacks > before.boundaryApplyCallbacks);
    LOKA_VERIFY(after.dirtySourceDeclarations > before.dirtySourceDeclarations);
    std::printf("paint baseline example %s: observations=%u visits=%lu declarations=%lu callbacks=%lu\n",
                name,
                platform.hintChecks,
                after.boundaryUpdateVisits - before.boundaryUpdateVisits,
                after.dirtySourceDeclarations - before.dirtySourceDeclarations,
                after.boundaryApplyCallbacks - before.boundaryApplyCallbacks);
    loka::dsl::testing::SceneTestAccess::unmount(scene);
  }
} // namespace

namespace
{
  void checkSurfaceBounds(BaselinePlatform::Observation expectation)
  {
    floppybird::SharedModel model;
    BaselinePlatform platform;
    Scene scene(Boundary<floppybird::MainNode>(floppybird::MainProps(&model)));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    settle(scene);
    BoundaryNode *root = SceneTestAccess::rootBoundary(scene);
    platform.applied.clear();
    if (expectation != BaselinePlatform::ABSENT_SURFACE_BOUNDS)
    {
      // Instrumented Null mount has no owner bounds. setLayoutBounds publishes
      // BOTH hint kinds; remove the paint hint only for the layout-only matrix arm.
      root->setLayoutBounds(7, 11, 320, 260);
      if (expectation == BaselinePlatform::LAYOUT_SURFACE_BOUNDS)
        root->updateResult().clearPaintBoundsHint();
    }
    platform.observation = expectation;
    const loka::app::scene::testing::PaintBaselineStats before = loka::app::scene::testing::paintBaselineStats();
    {
      loka::core::StateTrackerGuard guard(root->tracker());
      RectSurfaceModel next;
      next.rectCount = 1;
      next.rects[0].width = 10;
      next.rects[0].height = 10;
      model.surfaceModel_.set(next);
    }
    settle(scene);
    LOKA_VERIFY(platform.applied.size() == 1);
    const loka::app::scene::testing::PaintBaselineStats after = loka::app::scene::testing::paintBaselineStats();
    LOKA_VERIFY(after.boundaryUpdateVisits - before.boundaryUpdateVisits == 1);
    LOKA_VERIFY(after.boundaryApplyCallbacks - before.boundaryApplyCallbacks == 1);
    LOKA_VERIFY(after.dirtySourceDeclarations - before.dirtySourceDeclarations == 4);
    platform.observation = BaselinePlatform::OBSERVE_APPLIES;
    loka::dsl::testing::SceneTestAccess::unmount(scene);
  }

} // namespace

void testLegacySurfaceOnlyPropsDamagesWholeBoundary()
{
  checkSurfaceBounds(BaselinePlatform::LAYOUT_SURFACE_BOUNDS);
}

void testLegacyNullSurfacePropsHasNoBoundsHint()
{
  checkSurfaceBounds(BaselinePlatform::ABSENT_SURFACE_BOUNDS);
}

void testLegacySetLayoutBoundsPublishesPaintSpecificHint()
{
  checkSurfaceBounds(BaselinePlatform::PAINT_SURFACE_BOUNDS);
}

void testLegacyBoxComposeDeclaresPaintWithoutPaintSource()
{
  BaselinePlatform platform;
  platform.observation = BaselinePlatform::OBSERVE_BOX_PAINT;
  Scene scene((Boundary<BoxBoundary>()));
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  settle(scene);
  platform.applied.clear();
  scene.requestInvalidate(NODE_DIRTY_PROPS);
  LOKA_VERIFY(scene.flushInvalidation());
  LOKA_VERIFY(platform.applied.size() == 1);
  platform.observation = BaselinePlatform::OBSERVE_APPLIES;
  loka::dsl::testing::SceneTestAccess::unmount(scene);
}

void testLegacyExamplesHaveNoCompositedOrOpaquePaintHints()
{
  NullPlatformContext context;
  floppybird::SharedModel bird;
  smirkbench::SmirkModel smirk(640, 400, true);
  loka::core::EmitterState open, fit, actual, scroll;
  loka::core::MutableState<simpleviewer::DisplayMode> mode(simpleviewer::DISPLAY_FIT);
  checkExample("HelloWorld", Boundary<helloworld::MainNode>());
  checkExample("ScrapbookUI", Boundary<scrapbook::MainNode>(scrapbook::MainProps().platformContext(&context)));
  checkExample("MineSweeper", Boundary<minesweeper::MainNode>(minesweeper::MainProps(123)));
  checkExample("SimpleViewer",
               Boundary<simpleviewer::MainNode>(simpleviewer::MainProps()
                                                    .platformContext(&context)
                                                    .openDialogEvent(&open)
                                                    .displayMode(&mode)
                                                    .fitEvent(&fit)
                                                    .actualEvent(&actual)
                                                    .actualScrollEvent(&scroll)));
  checkExample("FloppyBird", Boundary<floppybird::MainNode>(floppybird::MainProps(&bird)));
  checkExample("SmirkBench", Boundary<smirkbench::MainNode>(smirkbench::MainProps(&smirk)));
}

void testLegacySiblingPaintPlansRejectSiblingRoots()
{
  BaselinePlatform platform;
  Scene scene((Boundary<PairBoundary>()));
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  settle(scene);
  BoundaryNode *a = firstChildBoundary(SceneTestAccess::rootBoundary(scene));
  BoundaryNode *b = a->nextInComposition->asBoundary();
  LOKA_VERIFY(b != 0);
  platform.foreignRoots.push_back(a);
  platform.foreignRoots.push_back(b);
  platform.applied.clear();
  a->markViewDirty(NODE_DIRTY_PROPS);
  b->markViewDirty(NODE_DIRTY_PROPS);
  LOKA_VERIFY(scene.flushInvalidation());
  LOKA_VERIFY(platform.applied.size() == 2);
  LOKA_VERIFY(platform.applied[0] != platform.applied[1]);
  platform.foreignRoots.clear();
  loka::dsl::testing::SceneTestAccess::unmount(scene);
}

void testLegacyParentPropsCompressesChildApply()
{
  checkCompression(NODE_DIRTY_PROPS, 1);
}
void testLegacyParentChildOnlyKeepsChildApply()
{
  checkCompression(NODE_DIRTY_CHILD, 2);
}
