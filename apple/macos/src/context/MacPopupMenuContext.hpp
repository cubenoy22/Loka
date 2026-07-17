#ifndef LOKA_MAC_POPUP_MENU_CONTEXT_HPP
#define LOKA_MAC_POPUP_MENU_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/nodes/controls/PopupMenu.hpp"

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

class MacPopupMenuContext : public loka::app::scene::NativeNodeContext
{
public:
  MacPopupMenuContext(void *parentView, int x, int y, int width, int height, loka::app::PopupMenuNode *node);
  virtual ~MacPopupMenuContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  /** Attach-time read (late-subscriber rule): presentation from the current
      fact, called by the installing handler right after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);

  void handleSelectionChange();
  void relayout(int x, int y, int width, int height);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
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
