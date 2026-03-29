#include "Win32PlatformHostActionLayoutHandlers.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/OpenFileDialog.hpp"

void RegisterWin32PlatformHostActionLayoutHandlers(Win32ScenePlatformController &controller)
{
  controller.hostActionHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>(),
      &Win32ScenePlatformController::dispatchOpenFileDialogLayout);
}
