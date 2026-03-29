#include "context/MacNodeContextMapper.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacCellContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacImageViewContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacPopupMenuContext.hpp"
#include "context/MacTextContext.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PopupMenu.hpp"
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

MacCellContext *MacNodeContextMapper::ensureCellContext(loka::app::CellNode *node,
                                                        int x,
                                                        int y,
                                                        int width,
                                                        int height) const
{
  if (!node)
  {
    return 0;
  }
  MacCellContext *ctx = static_cast<MacCellContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new MacCellContext(this->rootView_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}

MacEditTextContext *MacNodeContextMapper::ensureEditTextContext(loka::app::EditTextNode *node,
                                                                int x,
                                                                int y,
                                                                int width,
                                                                int height) const
{
  if (!node)
  {
    return 0;
  }
  MacEditTextContext *ctx = static_cast<MacEditTextContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new MacEditTextContext(this->rootView_, x, y, width, height, node);
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

MacOpenFileDialogContext *MacNodeContextMapper::ensureOpenFileDialogContext(loka::app::OpenFileDialogNode *node) const
{
  if (!node)
  {
    return 0;
  }
  MacOpenFileDialogContext *ctx = static_cast<MacOpenFileDialogContext *>(node->getContext());
  if (ctx)
  {
    return ctx;
  }
  ctx = new MacOpenFileDialogContext(this->rootView_, node);
  node->setContext(ctx);
  return ctx;
}

MacPopupMenuContext *MacNodeContextMapper::ensurePopupMenuContext(loka::app::PopupMenuNode *node,
                                                                  int x,
                                                                  int y,
                                                                  int width,
                                                                  int height) const
{
  if (!node)
  {
    return 0;
  }
  MacPopupMenuContext *ctx = static_cast<MacPopupMenuContext *>(node->getContext());
  if (ctx)
  {
    ctx->relayout(x, y, width, height);
    return ctx;
  }
  ctx = new MacPopupMenuContext(this->rootView_, x, y, width, height, node);
  node->setContext(ctx);
  return ctx;
}
