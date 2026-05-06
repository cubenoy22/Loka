#ifndef LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
#define LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "loka/core/Vector.hpp"
#include <Quickdraw.h>
#include <Menus.h>

class ToolboxScenePlatformController;
namespace loka
{
  namespace core
  {
    namespace scene
    {
      class BoundaryNode;
    }
  }
}
namespace loka
{
  namespace core
  {
    namespace scene
    {
      class IPlatformController;
    }
  }
}

class ToolboxPopupMenuContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit ToolboxPopupMenuContext(loka::app::PopupMenuNode *node);
  virtual ~ToolboxPopupMenuContext();

  void setBoundary(loka::app::scene::BoundaryNode *boundary) { boundary_ = boundary; }
  void invalidate();
  void updateData(const loka::Vector<loka::core::String> *items,
                  loka::core::State<int> *selectedIndex,
                  loka::core::EmitterState *onChange,
                  loka::core::State<bool> *enabled);
  void updateRect(const Rect &rect, short lineHeight);
  void draw();
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller,
                       loka::app::scene::LayoutState &state);
  bool handleMouseDown(const Point &point, ToolboxScenePlatformController *controller);

  const Rect &rect() const { return rect_; }

private:
  short clampIndex(int index) const;
  void copyToPascalString(const loka::core::String &value, Str255 out) const;
  short menuId() const;

  loka::app::PopupMenuNode *node_;
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
  short lineHeight_;
  const loka::Vector<loka::core::String> *items_;
  loka::core::State<int> *selectedIndex_;
  loka::core::EmitterState *onChange_;
  loka::core::State<bool> *enabled_;
};

#endif // LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
