#include "ToolboxScrollViewContractTests.hpp"

#include "support/TestVerify.hpp"

#include "../apple/toolbox/src/ToolboxScrollViewDecisions.hpp"

void testToolboxScrollViewChildWidthAlwaysReservesScrollbar()
{
  LOKA_VERIFY(ToolboxScrollViewChildWidth(100) == 84);
  LOKA_VERIFY(ToolboxScrollViewChildWidth(16) == 0);
  LOKA_VERIFY(ToolboxScrollViewChildWidth(8) == 0);
}

void testToolboxScrollViewHitIntersectionGatesViewport()
{
  const ToolboxScrollViewRect viewport(10, 20, 110, 120);
  ToolboxScrollViewRect result;

  LOKA_VERIFY(ToolboxScrollViewIntersectHitRect(
      ToolboxScrollViewRect(20, 30, 40, 50), viewport, result));
  LOKA_VERIFY(result.left == 20 && result.top == 30 &&
              result.right == 40 && result.bottom == 50);

  LOKA_VERIFY(ToolboxScrollViewIntersectHitRect(
      ToolboxScrollViewRect(0, 10, 30, 40), viewport, result));
  LOKA_VERIFY(result.left == 10 && result.top == 20 &&
              result.right == 30 && result.bottom == 40);

  LOKA_VERIFY(!ToolboxScrollViewIntersectHitRect(
      ToolboxScrollViewRect(0, 20, 10, 40), viewport, result));
  LOKA_VERIFY(!ToolboxScrollViewIntersectHitRect(
      ToolboxScrollViewRect(20, 120, 40, 140), viewport, result));
}

void testToolboxScrollViewOffsetClampRepublishesOnlyChangedFacts()
{
  ToolboxScrollViewMetrics metrics =
      ToolboxScrollViewResolveMetrics(300, 100, -7);
  LOKA_VERIFY(metrics.maximum == 200);
  LOKA_VERIFY(metrics.clampedOffset == 0);
  LOKA_VERIFY(ToolboxScrollViewShouldRepublish(-7, metrics.clampedOffset));

  metrics = ToolboxScrollViewResolveMetrics(300, 100, 75);
  LOKA_VERIFY(metrics.clampedOffset == 75);
  LOKA_VERIFY(!ToolboxScrollViewShouldRepublish(75, metrics.clampedOffset));

  metrics = ToolboxScrollViewResolveMetrics(300, 100, 50000);
  LOKA_VERIFY(metrics.clampedOffset == 200);
  LOKA_VERIFY(ToolboxScrollViewShouldRepublish(50000, metrics.clampedOffset));

  metrics = ToolboxScrollViewResolveMetrics(120, 200, 40);
  LOKA_VERIFY(metrics.maximum == 0);
  LOKA_VERIFY(metrics.clampedOffset == 0);
  LOKA_VERIFY(ToolboxScrollViewShouldRepublish(40, metrics.clampedOffset));
}

void testToolboxScrollViewRangeMapsContentMinusViewport()
{
  LOKA_VERIFY(ToolboxScrollViewMaximum(300, 100) == 200);
  LOKA_VERIFY(ToolboxScrollViewMaximum(100, 100) == 0);
  LOKA_VERIFY(ToolboxScrollViewMaximum(80, 100) == 0);
}
