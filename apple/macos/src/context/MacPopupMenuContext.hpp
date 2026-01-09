#ifndef LOKA_MAC_POPUP_MENU_CONTEXT_HPP
#define LOKA_MAC_POPUP_MENU_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/PopupMenu.hpp"

class MacPopupMenuContext : public declara::core::scene::NativeNodeContext
{
public:
  MacPopupMenuContext(void *parentView, int x, int y, int width, int height, declara::app::PopupMenuNode *node);
  virtual ~MacPopupMenuContext();

  void handleSelectionChange();

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
  void *popup_;
  void *target_;
  State<int> *selectionState_;
  State<bool> *enabledState_;
  bool applyingFromState_;
  bool updatingFromControl_;
};

#endif // LOKA_MAC_POPUP_MENU_CONTEXT_HPP
