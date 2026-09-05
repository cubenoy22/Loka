#include "ToolboxRectSurfaceRepaintPlanTests.hpp"
#include "../apple/toolbox/src/context/RectSurfaceRepaintPlan.hpp"
#include <cassert>

namespace
{
  using loka::app::RectSprite;
  using loka::app::RectSurfaceModel;
  using loka::core::Frame;
  using loka::toolbox::RectSurfaceRepaintPlan;

  const Frame surface(0, 0, 64, 64);

  bool contains(const Frame &rect, int x, int y)
  {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
  }

  bool covered(const RectSurfaceModel &model, int x, int y)
  {
    for (short i = 0; i < model.rectCount; ++i)
    {
      const RectSprite &r = model.rects[i];
      if (contains(Frame(r.x, r.y, r.width, r.height), x, y))
      {
        return true;
      }
    }
    return false;
  }

  bool commanded(const RectSurfaceRepaintPlan &plan, bool paint, int x, int y)
  {
    const short count = paint ? plan.paintCount() : plan.eraseCount();
    for (short i = 0; i < count; ++i)
    {
      if (contains(paint ? plan.paintRect(i) : plan.eraseRect(i), x, y))
      {
        return true;
      }
    }
    return false;
  }

  void
  verifyCoverage(const RectSurfaceModel &previous, const RectSurfaceModel &current, const RectSurfaceRepaintPlan &plan)
  {
    for (int y = 0; y < 64; ++y)
    {
      for (int x = 0; x < 64; ++x)
      {
        const bool painted = commanded(plan, true, x, y);
        const bool erased = commanded(plan, false, x, y);
        assert(!covered(current, x, y) || painted);
        assert(!covered(previous, x, y) || covered(current, x, y) || erased);
      }
    }
  }

  void apply(bool (&grid)[64][64], const RectSurfaceRepaintPlan &plan)
  {
    for (int y = 0; y < 64; ++y)
    {
      for (int x = 0; x < 64; ++x)
      {
        if (commanded(plan, false, x, y))
          grid[y][x] = false;
        if (commanded(plan, true, x, y))
          grid[y][x] = true;
      }
    }
  }
} // namespace

void testToolboxRepaintMovingSprite()
{
  RectSurfaceModel previous;
  previous.rectCount = 1;
  previous.rects[0] = RectSprite(8, 8, 24, 24);
  RectSurfaceModel current = previous;
  current.rects[0].x += 4;
  const RectSurfaceRepaintPlan plan(&previous, current, surface, surface, true);
  assert(plan.eraseCount() == 1);
  assert(plan.eraseRect(0) == Frame(8, 8, 4, 24));
  assert(plan.paintCount() == 1);
  assert(plan.paintRect(0) == Frame(12, 8, 24, 24));
  verifyCoverage(previous, current, plan);
}

void testToolboxRepaintOverlappingSprites()
{
  RectSurfaceModel previous;
  previous.rectCount = 2;
  previous.rects[0] = RectSprite(12, 12, 24, 24);
  previous.rects[1] = RectSprite(8, 10, 24, 24);
  RectSurfaceModel current = previous;
  current.rects[0] = RectSprite(15, 14, 24, 24);
  current.rects[1] = RectSprite(11, 12, 24, 24);
  const RectSurfaceRepaintPlan plan(&previous, current, surface, surface, true);
  assert(plan.eraseCount() == 4);
  assert(plan.eraseRect(0) == Frame(12, 12, 24, 2));
  assert(plan.eraseRect(1) == Frame(12, 14, 3, 22));
  assert(plan.eraseRect(2) == Frame(8, 10, 24, 2));
  assert(plan.eraseRect(3) == Frame(8, 12, 3, 22));
  verifyCoverage(previous, current, plan);
}

void testToolboxRepaintRemovedSprite()
{
  RectSurfaceModel previous;
  previous.rectCount = 2;
  previous.rects[0] = RectSprite(4, 4, 10, 10);
  previous.rects[1] = RectSprite(32, 32, 12, 12);
  RectSurfaceModel current = previous;
  current.rectCount = 1;
  const RectSurfaceRepaintPlan plan(&previous, current, surface, surface, true);
  assert(plan.eraseCount() == 1);
  assert(plan.eraseRect(0) == Frame(32, 32, 12, 12));
  verifyCoverage(previous, current, plan);
  current.rectCount = 0;
  const RectSurfaceRepaintPlan empty(&previous, current, surface, surface, true);
  assert(empty.eraseCount() == 2);
  assert(empty.paintCount() == 0);
  verifyCoverage(previous, current, empty);
}

void testToolboxRepaintWithoutPrevious()
{
  RectSurfaceModel current;
  current.rectCount = 1;
  current.rects[0] = RectSprite(0, 0, 24, 24);
  const RectSurfaceRepaintPlan plan(0, current, Frame(8, 10, 40, 40), Frame(4, 12, 20, 20), true);
  assert(plan.eraseCount() == 1);
  assert(plan.eraseRect(0) == Frame(8, 12, 16, 20));
  assert(plan.paintCount() == 1);
  assert(plan.paintRect(0) == Frame(8, 12, 16, 20));
}

void testToolboxRepaintTwentyOverlappingSteps()
{
  bool grid[64][64] = {};
  RectSurfaceModel previous;
  previous.rectCount = 12;
  for (short i = 0; i < 12; ++i)
  {
    previous.rects[i] = RectSprite(static_cast<short>(8 + (i % 4) * 3), static_cast<short>(8 + (i / 4) * 2), 24, 24);
  }
  const RectSurfaceRepaintPlan initial(0, previous, surface, surface, true);
  apply(grid, initial);
  for (int step = 0; step < 20; ++step)
  {
    RectSurfaceModel current = previous;
    for (short i = 0; i < 12; ++i)
    {
      const int direction = ((step / 4 + i) % 2) ? -1 : 1;
      current.rects[i].x += static_cast<short>(direction * 3);
      current.rects[i].y += static_cast<short>(direction * 2);
    }
    const RectSurfaceRepaintPlan plan(&previous, current, surface, surface, true);
    verifyCoverage(previous, current, plan);
    apply(grid, plan);
    for (int y = 0; y < 64; ++y)
    {
      for (int x = 0; x < 64; ++x)
      {
        assert(grid[y][x] == covered(current, x, y));
      }
    }
    previous = current;
  }
}

void testToolboxRepaintClippingAndCapacity()
{
  RectSurfaceModel previous;
  previous.rectCount = RectSurfaceModel::kMaxRects;
  RectSurfaceModel current = previous;
  for (short i = 0; i < previous.rectCount; ++i)
  {
    previous.rects[i] = RectSprite(4, 4, 24, 24);
    current.rects[i] = RectSprite(8, 8, 8, 8);
  }
  const RectSurfaceRepaintPlan full(&previous, current, surface, surface, true);
  assert(full.eraseCount() == 64);
  assert(full.paintCount() == 16);
  const Frame dirty(6, 6, 8, 8);
  const RectSurfaceRepaintPlan clipped(&previous, current, surface, dirty, true);
  for (int y = 0; y < 64; ++y)
  {
    for (int x = 0; x < 64; ++x)
    {
      assert(commanded(clipped, true, x, y) == (contains(dirty, x, y) && covered(current, x, y)));
      assert(commanded(clipped, false, x, y)
             == (contains(dirty, x, y) && covered(previous, x, y) && !covered(current, x, y)));
    }
  }
  const RectSurfaceRepaintPlan noClear(&previous, current, surface, surface, false);
  const RectSurfaceRepaintPlan firstNoClear(0, current, surface, surface, false);
  assert(noClear.eraseCount() == 0 && noClear.paintCount() == 16);
  assert(firstNoClear.eraseCount() == 0 && firstNoClear.paintCount() == 16);
  const RectSurfaceRepaintPlan outside(&previous, current, surface, Frame(60, 60, 4, 4), true);
  assert(outside.eraseCount() == 0 && outside.paintCount() == 0);
  previous.rectCount = 32767;
  current.rectCount = 32767;
  const RectSurfaceRepaintPlan bounded(&previous, current, surface, surface, true);
  assert(bounded.eraseCount() == 64 && bounded.paintCount() == 16);
}
