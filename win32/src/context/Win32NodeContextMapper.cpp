#include "context/Win32NodeContextMapper.hpp"
#include "context/Win32ButtonContext.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32ImageViewContext.hpp"
#include "context/Win32PopupMenuContext.hpp"
#include "context/Win32TextContext.hpp"
#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"

Win32ButtonContext *Win32NodeContextMapper::ensureButtonContext(loka::app::ButtonNode *node,
                                                                int x,
                                                                int y,
                                                                int width,
                                                                int height) const
{
  if (!node)
  {
    return 0;
  }
  Win32ButtonContext *ctx = static_cast<Win32ButtonContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new Win32ButtonContext(this->rootHwnd_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}

Win32EditTextContext *Win32NodeContextMapper::ensureEditTextContext(loka::app::EditTextNode *node,
                                                                    int x,
                                                                    int y,
                                                                    int width,
                                                                    int height) const
{
  if (!node)
  {
    return 0;
  }
  Win32EditTextContext *ctx = static_cast<Win32EditTextContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new Win32EditTextContext(this->rootHwnd_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}

Win32TextContext *Win32NodeContextMapper::ensureTextContext(loka::app::TextNode *node,
                                                            int x,
                                                            int y,
                                                            int width,
                                                            int height) const
{
  if (!node)
  {
    return 0;
  }
  Win32TextContext *ctx = static_cast<Win32TextContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new Win32TextContext(this->rootHwnd_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}

Win32ImageViewContext *Win32NodeContextMapper::ensureImageViewContext(loka::app::ImageViewNode *node,
                                                                      int x,
                                                                      int y,
                                                                      int width,
                                                                      int height) const
{
  if (!node)
  {
    return 0;
  }
  Win32ImageViewContext *ctx = static_cast<Win32ImageViewContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new Win32ImageViewContext(this->rootHwnd_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}

Win32PopupMenuContext *Win32NodeContextMapper::ensurePopupMenuContext(loka::app::PopupMenuNode *node,
                                                                      int x,
                                                                      int y,
                                                                      int width,
                                                                      int height) const
{
  if (!node)
  {
    return 0;
  }
  Win32PopupMenuContext *ctx = static_cast<Win32PopupMenuContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new Win32PopupMenuContext(this->rootHwnd_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}
