#ifndef LOKA_MAC_POPUP_MENU_CONTEXT_HPP
#define LOKA_MAC_POPUP_MENU_CONTEXT_HPP

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

class MacPopupMenuContext : public loka::app::scene::NativeNodeContext
{
public:
  MacPopupMenuContext(void *parentView, int x, int y, int width, int height, loka::app::PopupMenuNode *node);
  virtual ~MacPopupMenuContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  virtual void onNodeAttached();
  virtual void onNodeDetached();

  void handleSelectionChange();
  void relayout(int x, int y, int width, int height);

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
  void *popup_;
  void *target_;
  loka::core::State<int> *selectionState_;
  loka::core::State<bool> *enabledState_;
  bool applyingFromState_;
  bool updatingFromControl_;
};

void RegisterMacPopupMenuNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_POPUP_MENU_CONTEXT_HPP
