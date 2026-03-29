#include "Win32BuiltInSupport.hpp"
#include "Win32PlatformNodeHandlers.hpp"
#include "Win32PlatformLeafLayoutHandlers.hpp"
#include "Win32PlatformHostActionLayoutHandlers.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/Grid.hpp"
#include "app/RowColumn.hpp"
#include "app/Box.hpp"
#include "app/ZStack.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"

namespace
{
  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
  const int kPopupMenuHeight = 26;
  const int kTextHeight = 20;
  const int kHorizontalSpacing = 12;
  const int kImageFallbackHeightModern = 160;
}

void RegisterWin32BuiltInSupport(Win32ScenePlatformController &controller)
{
  loka::app::layout::RowLayoutMetrics rowMetrics;
  rowMetrics.gap = kHorizontalSpacing;
  rowMetrics.fallbackHeight = kTextHeight;
  rowMetrics.buttonHeight = kButtonHeight;
  rowMetrics.editTextHeight = kEditTextHeight;
  rowMetrics.popupMenuHeight = kPopupMenuHeight;
  rowMetrics.textHeight = kTextHeight;
  rowMetrics.imageFallbackHeight = kImageFallbackHeightModern;
  loka::app::layout::GridLayoutMetrics gridMetrics;
  gridMetrics.gapX = 0;
  gridMetrics.gapY = 0;
  loka::app::layout::RegisterBuiltinPlatformLayoutHandlers(controller.layoutHandlerRegistry_, &rowMetrics, &gridMetrics);
  RegisterWin32PlatformLeafLayoutHandlers(controller);
  RegisterWin32PlatformHostActionLayoutHandlers(controller);
  RegisterWin32PlatformNodeHandlers(controller.nodeHandlerRegistry_);
}
