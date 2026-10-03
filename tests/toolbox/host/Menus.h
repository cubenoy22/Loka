#ifndef LOKA_HOST_MENUS_H
#define LOKA_HOST_MENUS_H
#include "Controls.h"
namespace toolbox_host
{
  extern unsigned menuDraws, menuClears, menuDisposes, menuValueWrites;
  extern std::vector<MenuHandle> installedMenus;
}
enum { kInsertHierarchicalMenu = -1, hMenuCmd = 27 };
typedef short CharParameter;
inline short CountMenuItems(MenuHandle menu) { return static_cast<short>(menu->items.size()); }
inline void DeleteMenuItem(MenuHandle menu, short item) { menu->items.erase(menu->items.begin() + item - 1); }
inline void EnableItem(MenuHandle, short) { ++toolbox_host::menuValueWrites; }
inline void DisableItem(MenuHandle, short) { ++toolbox_host::menuValueWrites; }
inline void CheckItem(MenuHandle, short, bool) { ++toolbox_host::menuValueWrites; }
inline void ClearMenuBar() { ++toolbox_host::menuClears; toolbox_host::installedMenus.clear(); }
inline void InitMenus() {}
inline void DrawMenuBar() { ++toolbox_host::menuDraws; }
inline void AppendResMenu(MenuHandle, unsigned long) {}
inline void SetItemCmd(MenuHandle, short, short) {}
inline void SetItemMark(MenuHandle, short, CharParameter) {}
inline MenuHandle GetMenuHandle(short) { return 0; }
inline void GetMenuItemText(MenuHandle, short, unsigned char *text) { text[0] = 0; }
#endif
