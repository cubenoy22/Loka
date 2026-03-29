#include "MacPlatformLeafLayoutHandlers.hpp"
#include "MacScenePlatformController.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"

void RegisterMacPlatformLeafLayoutHandlers(MacScenePlatformController &controller)
{
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
}
