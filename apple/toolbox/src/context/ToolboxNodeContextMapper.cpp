#include "context/ToolboxNodeContextMapper.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxOpenFileDialogContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "context/ToolboxTextContext.hpp"
#include "context/ToolboxImageViewContext.hpp"
#include "ToolboxScenePlatformController.hpp"

void ToolboxNodeContextMapper::ensureTextContext(loka::app::TextNode *node)
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

void ToolboxNodeContextMapper::ensureCellContext(loka::app::CellNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxCellContext *ctx = static_cast<ToolboxCellContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxCellContext(node);
    node->setContext(ctx);
  }
}

void ToolboxNodeContextMapper::ensureButtonContext(loka::app::ButtonNode *node)
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

void ToolboxNodeContextMapper::ensureEditTextContext(loka::app::EditTextNode *node)
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

void ToolboxNodeContextMapper::ensureOpenFileDialogContext(loka::app::OpenFileDialogNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxOpenFileDialogContext *ctx = static_cast<ToolboxOpenFileDialogContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxOpenFileDialogContext(node);
    node->setContext(ctx);
  }
}

void ToolboxNodeContextMapper::ensurePopupMenuContext(loka::app::PopupMenuNode *node)
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

void ToolboxNodeContextMapper::ensureImageViewContext(loka::app::ImageViewNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxImageViewContext *ctx = static_cast<ToolboxImageViewContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxImageViewContext(node);
    node->setContext(ctx);
  }
}
