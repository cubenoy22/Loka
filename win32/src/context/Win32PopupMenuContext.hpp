#ifndef LOKA_WIN32_POPUP_MENU_CONTEXT_HPP
#define LOKA_WIN32_POPUP_MENU_CONTEXT_HPP

#include <windows.h>
#include "app/scene/NativeNodeContext.hpp"
#include "app/nodes/controls/PopupMenu.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  }
}

class Win32PopupMenuContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32PopupMenuContext(HWND parent, int x, int y, int width, int height, loka::app::PopupMenuNode *node);
  virtual ~Win32PopupMenuContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  virtual void onNodeAttached();
  virtual void onNodeDetached();

  HWND hwnd() const { return hwnd_; }
  void relayout(int x, int y, int width, int height);
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

  loka::app::PopupMenuNode *node_;
  HWND hwnd_;
  loka::core::State<int> *selectionState_;
  loka::core::State<bool> *enabledState_;
  bool applyingFromState_;
  bool updatingFromControl_;
  int baseHeight_;
  int baseWidth_;
};

void RegisterWin32PopupMenuNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_WIN32_POPUP_MENU_CONTEXT_HPP
