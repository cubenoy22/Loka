#include "context/ToolboxNodeContextMapper.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "context/ToolboxTextContext.hpp"
#include "ToolboxScenePlatformController.hpp"

void ToolboxNodeContextMapper::ensureTextContext(declara::app::TextNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxTextContext *ctx = static_cast<ToolboxTextContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxTextContext(node);
    node->setContext(ctx);
  }
}

void ToolboxNodeContextMapper::ensureButtonContext(declara::app::ButtonNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxButtonContext *ctx = static_cast<ToolboxButtonContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxButtonContext(node);
    node->setContext(ctx);
  }
}

void ToolboxNodeContextMapper::ensureEditTextContext(declara::app::EditTextNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxEditTextContext *ctx = static_cast<ToolboxEditTextContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxEditTextContext(node);
    node->setContext(ctx);
  }
}

void ToolboxNodeContextMapper::ensurePopupMenuContext(declara::app::PopupMenuNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxPopupMenuContext *ctx = static_cast<ToolboxPopupMenuContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxPopupMenuContext(node);
    node->setContext(ctx);
  }
}
