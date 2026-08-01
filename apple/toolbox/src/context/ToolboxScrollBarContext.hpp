#ifndef LOKA_TOOLBOX_SCROLL_BAR_CONTEXT_HPP
#define LOKA_TOOLBOX_SCROLL_BAR_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;
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
namespace loka
{
  namespace app
  {
    namespace scene
    {
      class BoundaryNode;
    }
  } // namespace app
} // namespace loka

/** Projects one ScrollBar node onto a scrollBarProc control. The control
    itself lives in the controller's binding table, keyed by the auto control
    id this context holds for its whole lifetime; the context owns only the
    rect and the retire ritual, exactly like ToolboxButtonContext. */
class ToolboxScrollBarContext : public ToolboxProjectedNodeContext
{
public:
  ToolboxScrollBarContext(loka::app::ScrollBarNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxScrollBarContext();

  void updateRect(const Rect &rect);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

  const Rect &rect() const
  {
    return rect_;
  }

private:
  loka::app::ScrollBarNode *node_;
  Rect rect_;
  short resourceId_;
  ToolboxScenePlatformController *controller_;
};

bool RegisterToolboxScrollBarNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_SCROLL_BAR_CONTEXT_HPP
