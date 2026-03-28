#include "MacPlatformNodeHandlers.hpp"
#include "MacScenePlatformController.hpp"
#include "app/ImageView.hpp"
#include "app/Text.hpp"
#include "context/MacImageViewContext.hpp"
#include "context/MacTextContext.hpp"

namespace
{
  class MacTextNodeHandler : public loka::app::scene::IPlatformNodeHandler
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
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!text || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureTextContext(text,
                                                     state.x,
                                                     state.y,
                                                     state.width,
                                                     state.height);
    }
  };

  class MacImageViewNodeHandler : public loka::app::scene::IPlatformNodeHandler
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
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!image || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureImageViewContext(image,
                                                          state.x,
                                                          state.y,
                                                          state.width,
                                                          state.height);
    }
  };

  MacTextNodeHandler gMacTextNodeHandler;
  MacImageViewNodeHandler gMacImageViewNodeHandler;
}

void RegisterMacPlatformNodeHandlers(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gMacTextNodeHandler);
  registry.registerHandler(&gMacImageViewNodeHandler);
}
