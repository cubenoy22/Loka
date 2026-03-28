#include "Win32PlatformNodeHandlers.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/ImageView.hpp"
#include "app/Text.hpp"
#include "context/Win32ImageViewContext.hpp"
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

  Win32TextNodeHandler gWin32TextNodeHandler;
  Win32ImageViewNodeHandler gWin32ImageViewNodeHandler;
}

void RegisterWin32PlatformNodeHandlers(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32TextNodeHandler);
  registry.registerHandler(&gWin32ImageViewNodeHandler);
}
