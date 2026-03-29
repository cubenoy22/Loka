#include "MacPlatformHostActionLayoutHandlers.hpp"
#include "MacScenePlatformController.hpp"
#include "app/OpenFileDialog.hpp"

void RegisterMacPlatformHostActionLayoutHandlers(MacScenePlatformController &controller)
{
  controller.hostActionHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>(),
      &MacScenePlatformController::dispatchOpenFileDialogLayout);
}
