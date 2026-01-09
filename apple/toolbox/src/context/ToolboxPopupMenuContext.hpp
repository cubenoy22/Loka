#ifndef LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
#define LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/PopupMenu.hpp"
#include "loka/core/Vector.hpp"
#include <Quickdraw.h>
#include <Menus.h>

class ToolboxScenePlatformController;

class ToolboxPopupMenuContext : public declara::core::scene::NativeNodeContext
{
public:
  explicit ToolboxPopupMenuContext(declara::app::PopupMenuNode *node);
  virtual ~ToolboxPopupMenuContext();

  void updateData(const loka::Vector<loka::core::String> *items,
                  declara::core::State<int> *selectedIndex,
                  declara::core::EmitterState *onChange,
                  declara::core::State<bool> *enabled);
  void updateRect(const Rect &rect);
  void draw(short lineHeight);
  bool handleMouseDown(const Point &point, ToolboxScenePlatformController *controller);

  const Rect &rect() const { return rect_; }

private:
  short clampIndex(int index) const;
  void copyToPascalString(const loka::core::String &value, Str255 out) const;

  declara::app::PopupMenuNode *node_;
  Rect rect_;
  const loka::Vector<loka::core::String> *items_;
  declara::core::State<int> *selectedIndex_;
  declara::core::EmitterState *onChange_;
  declara::core::State<bool> *enabled_;
};

#endif // LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
