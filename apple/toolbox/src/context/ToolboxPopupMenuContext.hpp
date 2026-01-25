#ifndef LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
#define LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/PopupMenu.hpp"
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

class ToolboxPopupMenuContext : public loka::core::scene::NativeNodeContext
{
public:
  explicit ToolboxPopupMenuContext(loka::app::PopupMenuNode *node);
  virtual ~ToolboxPopupMenuContext();

  void setBoundary(loka::core::scene::BoundaryNode *boundary) { boundary_ = boundary; }
  void updateData(const loka::Vector<loka::core::String> *items,
                  loka::core::State<int> *selectedIndex,
                  loka::core::EmitterState *onChange,
                  loka::core::State<bool> *enabled);
  void updateRect(const Rect &rect, short lineHeight);
  void draw();
  virtual void render(loka::core::scene::IPlatformController *controller);
  virtual short layout(loka::core::scene::IPlatformController *controller,
                       loka::core::scene::LayoutState &state);
  bool handleMouseDown(const Point &point, ToolboxScenePlatformController *controller);

  const Rect &rect() const { return rect_; }

private:
  short clampIndex(int index) const;
  void copyToPascalString(const loka::core::String &value, Str255 out) const;

  loka::app::PopupMenuNode *node_;
  loka::core::scene::BoundaryNode *boundary_;
  Rect rect_;
  short lineHeight_;
  const loka::Vector<loka::core::String> *items_;
  loka::core::State<int> *selectedIndex_;
  loka::core::EmitterState *onChange_;
  loka::core::State<bool> *enabled_;
};

#endif // LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
