#include "context/ToolboxNodeContextMapper.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxOpenFileDialogContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "context/ToolboxTextContext.hpp"
#include "context/ToolboxImageViewContext.hpp"
#include "context/ToolboxRectSurfaceContext.hpp"
#include "ToolboxScenePlatformController.hpp"

bool ToolboxNodeContextMapper::ensureProjectedContext(loka::app::scene::Node *node,
                                                      loka::app::scene::BoundaryNode *boundary,
                                                      ToolboxScenePlatformController *controller)
{
  if (!node)
  {
    return false;
  }
  if (loka::app::TextNode *text = node->asTextNode())
  {
    ensureTextContext(text);
    ToolboxTextContext *ctx = static_cast<ToolboxTextContext *>(text->getContext());
    if (!ctx)
    {
      return false;
    }
    ctx->setBoundary(boundary);
    return true;
  }
  if (loka::app::CellNode *cell = node->asCellNode())
  {
    ensureCellContext(cell);
    ToolboxCellContext *ctx = static_cast<ToolboxCellContext *>(cell->getContext());
    if (!ctx)
    {
      return false;
    }
    ctx->setBoundary(boundary);
    return true;
  }
  if (loka::app::ButtonNode *button = node->asButtonNode())
  {
    ensureButtonContext(button, controller);
    ToolboxButtonContext *ctx = static_cast<ToolboxButtonContext *>(button->getContext());
    if (!ctx)
    {
      return false;
    }
    ctx->setBoundary(boundary);
    return true;
  }
  if (loka::app::EditTextNode *edit = node->asEditTextNode())
  {
    ensureEditTextContext(edit);
    ToolboxEditTextContext *ctx = static_cast<ToolboxEditTextContext *>(edit->getContext());
    if (!ctx)
    {
      return false;
    }
    ctx->setBoundary(boundary);
    return true;
  }
  if (loka::app::PopupMenuNode *popup = node->asPopupMenuNode())
  {
    ensurePopupMenuContext(popup);
    ToolboxPopupMenuContext *ctx = static_cast<ToolboxPopupMenuContext *>(popup->getContext());
    if (!ctx)
    {
      return false;
    }
    ctx->setBoundary(boundary);
    return true;
  }
  if (loka::app::ImageViewNode *image = node->asImageViewNode())
  {
    ensureImageViewContext(image);
    ToolboxImageViewContext *ctx = static_cast<ToolboxImageViewContext *>(image->getContext());
    if (!ctx)
    {
      return false;
    }
    ctx->setBoundary(boundary);
    return true;
  }
  if (loka::app::OpenFileDialogNode *dialog = node->asOpenFileDialogNode())
  {
    ensureOpenFileDialogContext(dialog);
    return dialog->getContext() != 0;
  }
  return false;
}

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

void ToolboxNodeContextMapper::ensureButtonContext(loka::app::ButtonNode *node,
                                                   ToolboxScenePlatformController *controller)
{
  if (!node)
  {
    return;
  }
  ToolboxButtonContext *ctx = static_cast<ToolboxButtonContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxButtonContext(node, controller);
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
    // setContext's attach announce presents the newly materialized dialog;
    // re-presentation is owned by the fact channel. A second drive from the
    // projection sweep would re-fire after a mid-present close re-armed the
    // phase (the "presents twice, then never again" failure).
    node->setContext(ctx);
    return;
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

void ToolboxNodeContextMapper::ensureRectSurfaceContext(loka::app::RectSurfaceNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxRectSurfaceContext *ctx = static_cast<ToolboxRectSurfaceContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxRectSurfaceContext(node);
    node->setContext(ctx);
  }
}
