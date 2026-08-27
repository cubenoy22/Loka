#ifndef LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
#define LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "core/Vector.hpp"
#include <Quickdraw.h>
#include <Menus.h>

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
  namespace core
  {
    namespace scene
    {
      class BoundaryNode;
    }
  } // namespace core
} // namespace loka
namespace loka
{
  namespace core
  {
    namespace scene
    {
      class IPlatformController;
    }
  } // namespace core
} // namespace loka

class ToolboxPopupMenuContext : public ToolboxProjectedNodeContext
{
public:
  ToolboxPopupMenuContext(loka::app::PopupMenuNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxPopupMenuContext();

  void updateData(const loka::Vector<loka::core::String> *items,
                  loka::core::State<int> *selectedIndex,
                  loka::core::EmitterState *onChange,
                  loka::core::State<bool> *enabled);
  void updateRect(const Rect &rect, short lineHeight);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  bool handleMouseDown(const Point &point, ToolboxScenePlatformController *controller);

  const Rect &rect() const
  {
    return rect_;
  }

private:
  short clampIndex(int index) const;
  void copyToPascalString(const loka::core::String &value, Str255 out) const;
  short menuId() const;

  loka::app::PopupMenuNode *node_;
  Rect rect_;
  short lineHeight_;
  const loka::Vector<loka::core::String> *items_;
  loka::core::State<int> *selectedIndex_;
  loka::core::EmitterState *onChange_;
  loka::core::State<bool> *enabled_;
};

bool RegisterToolboxPopupMenuNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
