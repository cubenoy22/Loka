#include "platform/ToolboxPascalText.hpp"
/** Included by the native context and the host input fixture. */
bool ToolboxPopupMenuContext::handleMouseDown(const Point &point, ToolboxScenePlatformController *controller)
{
  if (enabled_ && !enabled_->get())
  {
    return false;
  }
  if (!items_ || items_->size() == 0 || !selectedIndex_)
  {
    return false;
  }
  if (!PtInRect(point, &rect_))
  {
    return false;
  }
  const loka::Vector<loka::core::String> *items = items_;
  loka::core::State<int> *selectedIndex = selectedIndex_;
  loka::core::EmitterState *onChange = onChange_;
  loka::app::scene::BoundaryNode *boundary = boundary_;
  Rect rect = rect_;
  short menuIdValue = menuId();
  static const unsigned char emptyTitle[] = {0};
  MenuHandle menu = NewMenu(menuIdValue, emptyTitle);
  if (!menu)
  {
    return false;
  }
  for (std::size_t j = 0; j < items->size(); ++j)
  {
    Str255 text;
    ToolboxEncodePascal((*items)[j], text);
    static const unsigned char placeholder[] = {1, ' '};
    AppendMenu(menu, placeholder);
    SetMenuItemText(menu, static_cast<short>(j + 1), text);
  }
  InsertMenu(menu, -1);
  short currentIndex = clampIndex(selectedIndex->get());
  Point globalPoint = point;
  LocalToGlobal(&globalPoint);
  long choice = PopUpMenuSelect(menu, globalPoint.v, globalPoint.h, static_cast<short>(currentIndex + 1));
  short item = static_cast<short>(choice & 0xFFFF);
  if (item > 0 && controller)
  {
    controller->applyPopupSelectionChange(rect, boundary, selectedIndex, selectedIndexSeat_, onChange, static_cast<int>(item - 1));
  }
  DeleteMenu(menuIdValue);
  DisposeMenu(menu);
  return true;
}
