#include "MacPlatformNodeHandlers.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacCellContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacImageViewContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacPopupMenuContext.hpp"
#include "context/MacTextContext.hpp"

void RegisterMacPlatformNodeHandlers(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  RegisterMacButtonNodeHandler(registry);
  RegisterMacTextNodeHandler(registry);
  RegisterMacImageViewNodeHandler(registry);
  RegisterMacEditTextNodeHandler(registry);
  RegisterMacPopupMenuNodeHandler(registry);
  RegisterMacCellNodeHandler(registry);
  RegisterMacOpenFileDialogNodeHandler(registry);
}
