#include "Win32PlatformNodeHandlers.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"
#include "context/Win32CellContext.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32ImageViewContext.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include "context/Win32PopupMenuContext.hpp"
#include "context/Win32TextContext.hpp"

namespace
{
  class Win32TextNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::TextNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::TextNode *text = node ? node->asTextNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!text || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureTextContext(text,
                                                       state.x,
                                                       state.y,
                                                       state.width,
                                                       state.height);
    }
  };

  class Win32ImageViewNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::ImageViewNode *image = node ? node->asImageViewNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!image || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureImageViewContext(image,
                                                            state.x,
                                                            state.y,
                                                            state.width,
                                                            state.height);
    }
  };

  class Win32EditTextNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::EditTextNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::EditTextNode *edit = node ? node->asEditTextNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!edit || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureEditTextContext(edit,
                                                           state.x,
                                                           state.y,
                                                           state.width,
                                                           state.height);
    }
  };

  class Win32PopupMenuNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::PopupMenuNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::PopupMenuNode *popup = node ? node->asPopupMenuNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!popup || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensurePopupMenuContext(popup,
                                                            state.x,
                                                            state.y,
                                                            state.width,
                                                            state.height);
    }
  };

  class Win32CellNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::CellNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::CellNode *cell = node ? node->asCellNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!cell || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureCellContext(cell,
                                                       state.x,
                                                       state.y,
                                                       state.width,
                                                       state.height);
    }
  };

  class Win32OpenFileDialogNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::OpenFileDialogNode *dialog = node ? node->asOpenFileDialogNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!dialog || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureOpenFileDialogContext(dialog);
    }
  };

  Win32TextNodeHandler gWin32TextNodeHandler;
  Win32ImageViewNodeHandler gWin32ImageViewNodeHandler;
  Win32EditTextNodeHandler gWin32EditTextNodeHandler;
  Win32PopupMenuNodeHandler gWin32PopupMenuNodeHandler;
  Win32CellNodeHandler gWin32CellNodeHandler;
  Win32OpenFileDialogNodeHandler gWin32OpenFileDialogNodeHandler;
}

void RegisterWin32PlatformNodeHandlers(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32TextNodeHandler);
  registry.registerHandler(&gWin32ImageViewNodeHandler);
  registry.registerHandler(&gWin32EditTextNodeHandler);
  registry.registerHandler(&gWin32PopupMenuNodeHandler);
  registry.registerHandler(&gWin32CellNodeHandler);
  registry.registerHandler(&gWin32OpenFileDialogNodeHandler);
}
