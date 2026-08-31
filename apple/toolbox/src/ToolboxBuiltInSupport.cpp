#include "ToolboxBuiltInSupport.hpp"
#include <cassert>
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "ToolboxWindowContext.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxImageViewContext.hpp"
#include "context/ToolboxOpenFileDialogContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "context/ToolboxScrollBarContext.hpp"
#include "context/ToolboxTextContext.hpp"
#include "app/nodes/nestable/ScrollView.hpp"

namespace
{
  loka::app::scene::RefusedNodeHandler gRefusedToolboxButton(
      loka::app::scene::NodeTypeToken<loka::app::ButtonNode>());
  loka::app::scene::RefusedNodeHandler gRefusedToolboxEditText(
      loka::app::scene::NodeTypeToken<loka::app::EditTextNode>());
  loka::app::scene::RefusedNodeHandler gRefusedToolboxScrollBar(
      loka::app::scene::NodeTypeToken<loka::app::ScrollBarNode>());
  loka::app::scene::RefusedNodeHandler gRefusedToolboxScrollView(
      loka::app::scene::NodeTypeToken<loka::app::ScrollViewNode>());
} // namespace

bool RegisterToolboxBuiltInSupport(ToolboxScenePlatformController &controller)
{
  assert(controller.window_ && controller.window_->context());
  const int capabilities = controller.window_->context()->capabilities();
  bool ok = true;
  if ((capabilities & ToolboxWindowContext::CAP_CONTROL_MANAGER) != 0)
  {
    ok = RegisterToolboxButtonNodeHandler(controller.nodeHandlerRegistry_) && ok;
  }
  else
  {
    ok = controller.nodeHandlerRegistry_.registerHandler(&gRefusedToolboxButton) && ok;
  }
  ok = RegisterToolboxTextNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxImageViewNodeHandler(controller.nodeHandlerRegistry_) && ok;
  if ((capabilities & ToolboxWindowContext::CAP_TEXT_EDIT) != 0)
  {
    ok = RegisterToolboxEditTextNodeHandler(controller.nodeHandlerRegistry_) && ok;
  }
  else
  {
    ok = controller.nodeHandlerRegistry_.registerHandler(&gRefusedToolboxEditText) && ok;
  }
  ok = RegisterToolboxPopupMenuNodeHandler(controller.nodeHandlerRegistry_) && ok;
  ok = RegisterToolboxCellNodeHandler(controller.nodeHandlerRegistry_) && ok;
  if ((capabilities & ToolboxWindowContext::CAP_CONTROL_MANAGER) != 0)
  {
    ok = RegisterToolboxScrollBarNodeHandler(controller.nodeHandlerRegistry_) && ok;
  }
  else
  {
    ok = controller.nodeHandlerRegistry_.registerHandler(&gRefusedToolboxScrollBar) && ok;
  }
  ok = controller.nodeHandlerRegistry_.registerHandler(&gRefusedToolboxScrollView) && ok;
  ok = RegisterToolboxOpenFileDialogNodeHandler(controller.nodeHandlerRegistry_) && ok;
  return ok;
}
