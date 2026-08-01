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

void RegisterToolboxBuiltInSupport(ToolboxScenePlatformController &controller)
{
  RegisterToolboxButtonNodeHandler(controller.nodeHandlerRegistry_);
  RegisterToolboxTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterToolboxImageViewNodeHandler(controller.nodeHandlerRegistry_);
  RegisterToolboxEditTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterToolboxPopupMenuNodeHandler(controller.nodeHandlerRegistry_);
  RegisterToolboxCellNodeHandler(controller.nodeHandlerRegistry_);
  RegisterToolboxScrollBarNodeHandler(controller.nodeHandlerRegistry_);
  RegisterToolboxOpenFileDialogNodeHandler(controller.nodeHandlerRegistry_);
}
