#include "Win32RailMetricsTests.hpp"
#include "support/TestVerify.hpp"
#include "Win32ScenePlatformController.hpp"
#include <climits>

void testWin32RailMetricsProjection()
{
  using namespace loka::app;
  using loka::win32::Win32DisplayScale;
  const RailMetrics defaults;
  LOKA_VERIFY(defaults.fontScale == Ratio(1, 1));
  LOKA_VERIFY(defaults.spaceScale == Ratio(1, 1));
  LOKA_VERIFY(!Ratio(3, 2).isUnit());
  Ratio invalid;
  invalid.den = 0;
  LOKA_VERIFY(!invalid.valid());
  invalid.den = -1;
  LOKA_VERIFY(!invalid.valid());

  const Win32DisplayScale unit(144, defaults);
  const int coordinates[] = {-301, -33, -9, -3, -1, 0, 1, 3, 8, 9, 33, 301};
  for (unsigned int i = 0; i < sizeof(coordinates) / sizeof(coordinates[0]); ++i)
  {
    const int value = coordinates[i];
    // Pin today's MulDiv rule directly, including negative edges and ties.
    LOKA_VERIFY(unit.projectEdge(value) == MulDiv(value, 144, 96));
    LOKA_VERIFY(unit.projectLength(value) == MulDiv(value, 144, 96));
    LOKA_VERIFY(unit.unprojectEdge(value) == MulDiv(value, 96, 144));
    LOKA_VERIFY(unit.unprojectLength(value) == MulDiv(value, 96, 144));
    const loka::core::Frame frame(value, -value, 9, 3);
    RECT projected;
    unit.projectFrame(frame, projected);
    LOKA_VERIFY(projected.left == MulDiv(value, 144, 96));
    LOKA_VERIFY(projected.top == MulDiv(-value, 144, 96));
    LOKA_VERIFY(projected.right == MulDiv(value + 9, 144, 96));
    LOKA_VERIFY(projected.bottom == MulDiv(-value + 3, 144, 96));
  }

  const RailMetrics metrics(Ratio(5, 4), Ratio(3, 2));
  const Win32DisplayScale scaled(96, metrics);
  LOKA_VERIFY(scaled.dpi() == 96 && scaled.percent() == 100);
  LOKA_VERIFY(scaled.projectEdge(8) == 12);
  LOKA_VERIFY(scaled.projectEdge(9) == 14);
  LOKA_VERIFY(scaled.projectLength(9) == 14);
  LOKA_VERIFY(scaled.projectEdge(-9) == -14);
  LOKA_VERIFY(scaled.unprojectEdge(12) == 8);
  // Today's nearest inverse; floor-capacity containment belongs to PR a'.
  LOKA_VERIFY(scaled.unprojectLength(301) == 201);
  RECT projected;
  scaled.projectFrame(loka::core::Frame(-9, 3, 9, 5), projected);
  LOKA_VERIFY(projected.left == -14 && projected.top == 5);
  LOKA_VERIFY(projected.right == 0 && projected.bottom == 12);
  LOKA_VERIFY(scaled != Win32DisplayScale(96));
  LOKA_VERIFY(scaled == Win32DisplayScale(96, metrics));
  // Non-client/native metrics remain DPI-only, even with non-unit rail metrics.
  LOKA_VERIFY(scaled.scaleLengthFrom(Win32DisplayScale(144), 12) == 8);
  const Win32DisplayScale equivalentUnit(144, RailMetrics(Ratio(), Ratio(INT_MAX, INT_MAX)));
  LOKA_VERIFY(equivalentUnit.projectEdge(9) == unit.projectEdge(9));
#ifdef NDEBUG
  const Win32DisplayScale overflow(144, RailMetrics(Ratio(), Ratio(INT_MAX, 1)));
  LOKA_VERIFY(overflow.projectEdge(1) == -1);
  LOKA_VERIFY(overflow.unprojectEdge(1) == -1);
  const Win32DisplayScale denominatorOverflow(96, RailMetrics(Ratio(), Ratio(1, INT_MAX)));
  LOKA_VERIFY(denominatorOverflow.projectLength(1) == -1);
  LOKA_VERIFY(!Ratio(1, 0).valid());
  LOKA_VERIFY(!Ratio(1, -1).valid());
#else
  std::printf("[skip] Win32 overflow refusal pins require NDEBUG; Ratio debug death pin runs on Linux.\n");
#endif
}

void testWin32RailMetricsSurviveDpiChange()
{
  using namespace loka::app;
  using loka::win32::Win32DisplayScale;
  const RailMetrics metrics(Ratio(5, 4), Ratio(3, 2));
  // No native subtree is needed to exercise the controller's DPI/font update.
  Win32ScenePlatformController controller(NULL, Win32DisplayScale(96, metrics));
  LOKA_VERIFY(controller.displayScale().railMetrics() == metrics);
  LOGFONTW font;
  LOKA_VERIFY(GetObjectW(controller.textFont(FontSize<24>()), sizeof(font), &font));
  LOKA_VERIFY(font.lfHeight == -MulDiv(24, 96 * 5, 72 * 4));
  // A font-only change must not compare equal in the font-table cache.
  loka::win32::Win32DisplayFont table;
  LOKA_VERIFY(table.create(Win32DisplayScale(96)));
  LOKA_VERIFY(!table.matches(Win32DisplayScale(96, RailMetrics(Ratio(3, 2), Ratio()))));
  LOGFONTW nativeFont;
  LOKA_VERIFY(GetObjectW(table.get(), sizeof(nativeFont), &nativeFont));
  LOKA_VERIFY(GetObjectW(controller.displayFont(), sizeof(font), &font));
  LOKA_VERIFY(font.lfHeight == nativeFont.lfHeight);

  controller.updateDisplayScale(Win32DisplayScale(144));
  LOKA_VERIFY(controller.displayScale().dpi() == 144);
  LOKA_VERIFY(controller.displayScale().percent() == 150);
  LOKA_VERIFY(controller.displayScale().railMetrics() == metrics);
  LOKA_VERIFY(controller.displayScale().projectEdge(8) == 18);
  LOKA_VERIFY(GetObjectW(controller.textFont(FontSize<24>()), sizeof(font), &font));
  LOKA_VERIFY(font.lfHeight == -MulDiv(24, 144 * 5, 72 * 4));
  controller.updateDisplayScale(Win32DisplayScale(96));
  LOKA_VERIFY(controller.displayScale().railMetrics() == metrics);
  LOKA_VERIFY(controller.displayScale().projectEdge(8) == 12);
}
