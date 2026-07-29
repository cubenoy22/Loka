#ifndef LOKA_TOOLBOX_SCROLL_BAR_CONTEXT_HPP
#define LOKA_TOOLBOX_SCROLL_BAR_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;
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
class ToolboxScrollBarContext : public loka::app::scene::NativeNodeContext
{
public:
  ToolboxScrollBarContext(loka::app::ScrollBarNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxScrollBarContext();

  void setBoundary(loka::app::scene::BoundaryNode *boundary)
  {
    boundary_ = boundary;
  }
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
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
  short resourceId_;
  ToolboxScenePlatformController *controller_;
};

#endif // LOKA_TOOLBOX_SCROLL_BAR_CONTEXT_HPP
