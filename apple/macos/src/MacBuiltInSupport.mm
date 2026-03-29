#include "MacBuiltInSupport.hpp"
#include "MacScenePlatformController.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacCellContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacImageViewContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacPopupMenuContext.hpp"
#include "context/MacTextContext.hpp"

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
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::ButtonNode>(),
      &MacScenePlatformController::dispatchButtonLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::EditTextNode>(),
      &MacScenePlatformController::dispatchEditTextLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::PopupMenuNode>(),
      &MacScenePlatformController::dispatchPopupMenuLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::CellNode>(),
      &MacScenePlatformController::dispatchCellLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::TextNode>(),
      &MacScenePlatformController::dispatchTextLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>(),
      &MacScenePlatformController::dispatchImageViewLayout);
  controller.hostActionHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>(),
      &MacScenePlatformController::dispatchOpenFileDialogLayout);
  RegisterMacButtonNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacImageViewNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacEditTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacPopupMenuNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacCellNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacOpenFileDialogNodeHandler(controller.nodeHandlerRegistry_);
}
