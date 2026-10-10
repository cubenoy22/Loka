#include "ToolboxGroundTests.hpp"
#include "../apple/toolbox/src/ToolboxGround.hpp"
#include "support/ContrastRatio.hpp"
#include "support/TestVerify.hpp"

namespace
{
  using namespace loka::toolbox;
  double PlanarLuminance(ToolboxPlanarColor color, bool monochrome)
  {
    if (monochrome)
      return color == TOOLBOX_PLANAR_WHITE ? 1.0 : 0.0;
    switch (color)
    {
    case TOOLBOX_PLANAR_WHITE:
      return loka_test::RelativeLuminance(1.0, 1.0, 1.0);
    case TOOLBOX_PLANAR_BLACK:
      return loka_test::RelativeLuminance(0.0, 0.0, 0.0);
    }
    LOKA_VERIFY(false);
    return 0.0;
  }
}

void testToolboxGroundRoles()
{
  using namespace loka::app;
  ToolboxPlanarColor color = TOOLBOX_PLANAR_BLACK;
  LOKA_VERIFY(!QueryToolboxGroundColor(SURFACE_GROUND_TRANSPARENT, color));
  LOKA_VERIFY(color == TOOLBOX_PLANAR_BLACK);
  LOKA_VERIFY(!QueryToolboxGroundColor(SURFACE_GROUND_NATIVE, color));
  LOKA_VERIFY(color == TOOLBOX_PLANAR_BLACK);
  LOKA_VERIFY(QueryToolboxGroundColor(SURFACE_GROUND_WINDOW, color));
  LOKA_VERIFY(color == TOOLBOX_PLANAR_WHITE);
  LOKA_VERIFY(QueryToolboxGroundColor(SURFACE_GROUND_DOCUMENT, color));
  LOKA_VERIFY(color == TOOLBOX_PLANAR_WHITE);
  LOKA_VERIFY(QueryToolboxGroundColor(SURFACE_GROUND_CONTROL, color));
  LOKA_VERIFY(color == TOOLBOX_PLANAR_WHITE);
}

void testToolboxGroundLegibility()
{
  using namespace loka::app;
  const SurfaceGround roles[] = {SURFACE_GROUND_TRANSPARENT, SURFACE_GROUND_NATIVE,
      SURFACE_GROUND_WINDOW, SURFACE_GROUND_DOCUMENT, SURFACE_GROUND_CONTROL};
  // Until #1196 A2 supplies text roles, every drawer uses QuickDraw's default
  // blackColor foreground: this is the single text row, in both display columns.
  for (unsigned i = 0; i < sizeof(roles) / sizeof(roles[0]); ++i)
  {
    ToolboxPlanarColor ground;
    if (!QueryToolboxGroundColor(roles[i], ground))
      continue;
    for (int column = 0; column < 2; ++column)
    {
      const double ratio = loka_test::ContrastRatio(
          PlanarLuminance(TOOLBOX_PLANAR_BLACK, column == 0), PlanarLuminance(ground, column == 0));
      LOKA_VERIFY(ratio >= 4.5);
    }
  }
}
