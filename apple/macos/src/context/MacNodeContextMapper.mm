#include "context/MacNodeContextMapper.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacImageViewContext.hpp"
#include "context/MacTextContext.hpp"
#include "app/Button.hpp"
#include "app/ImageView.hpp"
#include "app/Text.hpp"

MacButtonContext *MacNodeContextMapper::ensureButtonContext(loka::app::ButtonNode *node,
                                                            int x,
                                                            int y,
                                                            int width,
                                                            int height) const
{
  if (!node)
  {
    return 0;
  }
  MacButtonContext *ctx = static_cast<MacButtonContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new MacButtonContext(this->rootView_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}

MacTextContext *MacNodeContextMapper::ensureTextContext(loka::app::TextNode *node,
                                                        int x,
                                                        int y,
                                                        int width,
                                                        int height) const
{
  if (!node)
  {
    return 0;
  }
  MacTextContext *ctx = static_cast<MacTextContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new MacTextContext(this->rootView_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}

MacImageViewContext *MacNodeContextMapper::ensureImageViewContext(loka::app::ImageViewNode *node,
                                                                  int x,
                                                                  int y,
                                                                  int width,
                                                                  int height) const
{
  if (!node)
  {
    return 0;
  }
  MacImageViewContext *ctx = static_cast<MacImageViewContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new MacImageViewContext(this->rootView_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}
