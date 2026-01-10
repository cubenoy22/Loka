#include "context/ToolboxNodeContextMapper.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "context/ToolboxTextContext.hpp"
#include "ToolboxScenePlatformController.hpp"

void ToolboxNodeContextMapper::ensureTextContext(declara::app::TextNode *node,
                                                 const Rect &rect,
                                                 short textX,
                                                 short textY,
                                                 ToolboxScenePlatformController *controller)
{
  if (!node || !controller)
  {
    return;
  }
  ToolboxTextContext *ctx = static_cast<ToolboxTextContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxTextContext(node);
    node->setContext(ctx);
  }
  ctx->updateData(node->props.text_);
  ctx->updateRect(rect, textX, textY);
}

void ToolboxNodeContextMapper::ensureButtonContext(declara::app::ButtonNode *node,
                                                   const Rect &rect,
                                                   const loka::core::String &label,
                                                   ToolboxScenePlatformController *controller)
{
  if (!node || !controller)
  {
    return;
  }
  if (!hasCapability(CAP_CONTROL_MANAGER))
  {
    return;
  }
  ToolboxButtonContext *ctx = static_cast<ToolboxButtonContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxButtonContext(node);
    node->setContext(ctx);
  }
  ctx->updateData(label, node->props.onClick_, node->props.enabled_, node->props.toolboxControlId_);
  ctx->updateRect(rect);
}

void ToolboxNodeContextMapper::ensureEditTextContext(declara::app::EditTextNode *node,
                                                     const Rect &rect,
                                                     const Rect &textRect,
                                                     short textX,
                                                     short textY,
                                                     ToolboxScenePlatformController *controller)
{
  if (!node || !controller)
  {
    return;
  }
  if (!hasCapability(CAP_TEXT_EDIT))
  {
    return;
  }
  ToolboxEditTextContext *ctx = static_cast<ToolboxEditTextContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxEditTextContext(node);
    node->setContext(ctx);
  }
  ctx->updateData(node->props.text_);
  ctx->updateRect(rect, textRect, textX, textY);
}

void ToolboxNodeContextMapper::ensurePopupMenuContext(declara::app::PopupMenuNode *node,
                                                      const Rect &rect,
                                                      short lineHeight,
                                                      ToolboxScenePlatformController *controller)
{
  if (!node || !controller)
  {
    return;
  }
  ToolboxPopupMenuContext *ctx = static_cast<ToolboxPopupMenuContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxPopupMenuContext(node);
    node->setContext(ctx);
  }
  ctx->updateData(node->props.items_, node->props.selectedIndex_, node->props.onChange_, node->props.enabled_);
  ctx->updateRect(rect, lineHeight);
  controller->registerPopupContext(ctx);
}

void ToolboxNodeContextMapper::renderTextContext(declara::app::TextNode *node,
                                                 ToolboxScenePlatformController *controller)
{
  if (!node)
  {
    return;
  }
  ToolboxTextContext *ctx = static_cast<ToolboxTextContext *>(node->getContext());
  if (ctx)
  {
    ctx->draw(controller);
  }
}

void ToolboxNodeContextMapper::renderButtonContext(declara::app::ButtonNode *node,
                                                   ToolboxScenePlatformController *controller)
{
  if (!node)
  {
    return;
  }
  ToolboxButtonContext *ctx = static_cast<ToolboxButtonContext *>(node->getContext());
  if (ctx)
  {
    ctx->draw(controller);
  }
}

void ToolboxNodeContextMapper::renderEditTextContext(declara::app::EditTextNode *node,
                                                     ToolboxScenePlatformController *controller)
{
  if (!node)
  {
    return;
  }
  ToolboxEditTextContext *ctx = static_cast<ToolboxEditTextContext *>(node->getContext());
  if (ctx)
  {
    ctx->draw(controller);
  }
}

void ToolboxNodeContextMapper::renderPopupMenuContext(declara::app::PopupMenuNode *node)
{
  if (!node)
  {
    return;
  }
  ToolboxPopupMenuContext *ctx = static_cast<ToolboxPopupMenuContext *>(node->getContext());
  if (ctx)
  {
    ctx->draw();
  }
}
