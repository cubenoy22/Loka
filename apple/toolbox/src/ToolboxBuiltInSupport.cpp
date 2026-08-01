#include "ToolboxBuiltInSupport.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxImageViewContext.hpp"
#include "context/ToolboxOpenFileDialogContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "context/ToolboxScrollBarContext.hpp"
#include "context/ToolboxTextContext.hpp"

bool RegisterToolboxBuiltInSupport(ToolboxScenePlatformController &controller)
{
  bool ok = true;
  ok = RegisterToolboxButtonNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxTextNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxImageViewNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxEditTextNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxPopupMenuNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxCellNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxScrollBarNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxOpenFileDialogNodeHandler(controller.nodeHandlerRegistry_) && ok;
  return ok;
}
