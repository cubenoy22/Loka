#ifndef LOKA_TOOLBOX_APP_HPP
#define LOKA_TOOLBOX_APP_HPP

#include "core/App.hpp"
#include <vector>
#include <Menus.h>

class ToolboxApp : public App
{
protected:
  explicit ToolboxApp(AppConfigurable *config);
  virtual ~ToolboxApp();
  friend class ToolboxPlatformContext;

public:
  virtual void run();
  virtual void quit();
  virtual void windowClosed(Window *window);
  void handleMenuCommand(short menuId, short item);
  static void MenuEnabledChangedThunk(void *userData);

public:
  virtual void applyMenuBar(Window *activeWindow);

  struct MenuCommand
  {
    short menuId;
    short itemIndex;
    declara::app::MenuActionType action;
    declara::core::EmitterState *emitter;
  };

  struct MenuBinding
  {
    MenuHandle menu;
    short itemIndex;
    State<bool> *enabledState;
  };

private:
  struct MenuEntry
  {
    MenuHandle menu;
    short menuId;
    bool isAppMenu;
    loka::core::String title;
  };

  void clearMenuBindings();
  void clearMenuBindingsFor(MenuHandle menuHandle, short menuId);
  void resetMenuState();
  void disposeMenuEntries();
  short nextMenuId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  std::vector<MenuEntry> menuEntries_;
  bool running_;
};

#endif // LOKA_TOOLBOX_APP_HPP
