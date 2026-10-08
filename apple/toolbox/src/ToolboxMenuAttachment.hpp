#ifndef LOKA_TOOLBOX_MENU_ATTACHMENT_HPP
#define LOKA_TOOLBOX_MENU_ATTACHMENT_HPP

#include "app/Menu.hpp"
#include "core/util/OwnedDef.hpp"
#include <Menus.h>
#include <vector>

class ToolboxApp;
namespace loka { namespace app { namespace scene { class Scene; } } }

/** Owns the global Classic menu projection. Scene detach revokes all endpoint
    borrows synchronously; native handles remain inert until replacement/disposal. */
class ToolboxMenuAttachment
{
public:
  explicit ToolboxMenuAttachment(ToolboxApp &app);
  ~ToolboxMenuAttachment();
  /** Returns true when native menus were projected; equal offers return false.
      Snapshot refusal clears the baseline without dropping the projection. */
  bool project(const loka::app::MenuBarDefinition *bar,
               const loka::app::scene::Scene *source);
  void disconnect();
  void releaseFrom(const loka::app::scene::Scene *scene);
  bool dispatch(short menuId, short item);

private:
  ToolboxMenuAttachment(const ToolboxMenuAttachment &);
  ToolboxMenuAttachment &operator=(const ToolboxMenuAttachment &);
  struct MenuCommand
  {
    short menuId;
    short itemIndex;
    loka::app::MenuActionType action;
    loka::core::EmitterState *emitter;
  };
  struct MenuBinding
  {
    ToolboxApp *app;
    MenuHandle menu;
    short itemIndex;
    loka::core::State<loka::core::String> *titleState;
    loka::core::State<bool> *enabledState;
    bool invertEnabled;
    loka::core::State<bool> *checkedState;
  };
  struct MenuEntry
  {
    MenuHandle menu;
    short menuId;
    bool isAppMenu;
    loka::core::String title;
  };
  static void MenuTitleChangedThunk(void *userData);
  static void MenuEnabledChangedThunk(void *userData);
  static void MenuCheckedChangedThunk(void *userData);
  static void ApplyMenuItemStates(ToolboxApp *app, MenuHandle menu, short itemIndex,
                                 const loka::app::MenuItemDefinition *item,
                                 std::vector<MenuBinding *> &bindings);
  static void BuildMenuItems(ToolboxApp *app, MenuHandle menu,
                            const loka::app::MenuItemDefinition *items, short menuId,
                            short &nextMenuId, std::vector<MenuCommand> &commands,
                            std::vector<MenuBinding *> &bindings,
                            std::vector<MenuHandle> &hierarchicalMenus);
  static bool HasHierarchicalItems(const loka::app::MenuItemDefinition *items);
  void clearMenuBindings();
  void clearMenuBindingsFor(MenuHandle menu, short menuId);
  void resetMenuState();
  void disposeMenuEntries();
  void disposeHierarchicalMenus();

  ToolboxApp &app_;
  short nextMenuId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  std::vector<MenuEntry> menuEntries_;
  std::vector<MenuHandle> hierarchicalMenus_;
  loka::core::OwnedDef<loka::app::MenuBarDefinition> applied_;
  const loka::app::scene::Scene *source_;
};
#endif
