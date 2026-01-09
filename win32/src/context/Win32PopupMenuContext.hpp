#ifndef LOKA_WIN32_POPUP_MENU_CONTEXT_HPP
#define LOKA_WIN32_POPUP_MENU_CONTEXT_HPP

#include <windows.h>
#include "core2/scene/NativeNodeContext.hpp"
#include "app/PopupMenu.hpp"

class Win32PopupMenuContext : public declara::core::scene::NativeNodeContext
{
public:
  Win32PopupMenuContext(HWND parent, int x, int y, int width, int height, declara::app::PopupMenuNode *node);
  virtual ~Win32PopupMenuContext();

  HWND hwnd() const { return hwnd_; }
  bool handleCommand(WPARAM wParam, LPARAM lParam);

private:
  void bindSelection();
  void unbindSelection();
  void bindEnabled();
  void unbindEnabled();
  void applyItems();
  void applySelection();
  void applyEnabled();
  void syncStateFromControl();

  static void SelectionChangedThunk(void *userData);
  static void EnabledChangedThunk(void *userData);

  declara::app::PopupMenuNode *node_;
  HWND hwnd_;
  State<int> *selectionState_;
  State<bool> *enabledState_;
  bool applyingFromState_;
  bool updatingFromControl_;
  int baseHeight_;
  int baseWidth_;
};

#endif // LOKA_WIN32_POPUP_MENU_CONTEXT_HPP
