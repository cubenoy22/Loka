#ifndef LOKA_MAC_MENU_ATTACHMENT_HPP
#define LOKA_MAC_MENU_ATTACHMENT_HPP

#include "app/Menu.hpp"
#include "core/util/OwnedDef.hpp"
#include <vector>

class MacApp;
namespace loka { namespace app { namespace scene { class Scene; } } }

/** Owns the global AppKit menu projection and its revocable endpoint borrows.
    Scene detach leaves installed and tracking-retained native items inert. */
class MacMenuAttachment
{
public:
  explicit MacMenuAttachment(MacApp &app);
  ~MacMenuAttachment();
  /** Returns true when projected; equal source/projection offers do nothing.
      Snapshot refusal clears the baseline without dropping the projection. */
  bool project(const loka::app::MenuBarDefinition *bar,
               const loka::app::scene::Scene *source);
  void disconnect();
  void releaseFrom(const loka::app::scene::Scene *scene);
  bool dispatch(int commandId);

private:
  MacMenuAttachment(const MacMenuAttachment &);
  MacMenuAttachment &operator=(const MacMenuAttachment &);
  struct MenuCommand
  {
    int commandId;
    loka::app::MenuActionType action;
    loka::core::EmitterState *emitter;
  };
  struct MenuBinding
  {
    void *menuItem;
    loka::core::State<bool> *enabledState;
    bool invertEnabled;
    loka::core::State<bool> *checkedState;
  };
  static void MenuEnabledChangedThunk(void *userData);
  static void MenuCheckedChangedThunk(void *userData);
  std::size_t BuildMenuItem(void *menu, const loka::app::MenuItemDefinition *item, bool allowQuit);
  std::size_t BuildMenuItems(void *menu, const loka::app::MenuItemDefinition *items, bool allowQuit);
  void clearMenuBindings();
  void reset();

  MacApp &app_;
  int nextCommandId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  void *target_;
  void *menu_;
  loka::core::OwnedDef<loka::app::MenuBarDefinition> applied_;
  const loka::app::scene::Scene *source_;
};
#endif
