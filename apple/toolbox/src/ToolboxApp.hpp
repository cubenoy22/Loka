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
  void clearMenuBindings();
  short nextMenuId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  bool running_;
};

#endif // LOKA_TOOLBOX_APP_HPP
