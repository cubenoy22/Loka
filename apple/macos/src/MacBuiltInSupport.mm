#include "MacBuiltInSupport.hpp"
#include "MacPlatformNodeHandlers.hpp"
#include "MacPlatformLeafLayoutHandlers.hpp"
#include "MacPlatformHostActionLayoutHandlers.hpp"
#include "MacScenePlatformController.hpp"
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

void RegisterMacBuiltInSupport(MacScenePlatformController &controller)
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
  RegisterMacPlatformLeafLayoutHandlers(controller);
  RegisterMacPlatformHostActionLayoutHandlers(controller);
  RegisterMacPlatformNodeHandlers(controller.nodeHandlerRegistry_);
}
