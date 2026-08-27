#ifndef LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP
#define LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/ImageView.hpp"
#include "core/resource/Image.hpp"
#include "core/State.hpp"
#include <Quickdraw.h>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka

class ToolboxImageViewContext : public ToolboxProjectedNodeContext
{
public:
  ToolboxImageViewContext(loka::app::ImageViewNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxImageViewContext();

  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state);
  virtual void render(loka::app::scene::IPlatformController *);

private:
  void draw();
  void updateRect(short x, short y, short width, short height);

  loka::app::ImageViewNode *node_;
  Rect rect_;
};

bool RegisterToolboxImageViewNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP
