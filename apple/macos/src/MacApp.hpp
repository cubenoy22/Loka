#ifndef LOKA_MAC_APP_HPP
#define LOKA_MAC_APP_HPP

#include "core/App.hpp"
#include <vector>

class MacWindow;

class MacApp : public App
{
public:
  explicit MacApp(AppConfigurable *config);
  virtual ~MacApp();

  virtual void run();
  virtual void quit();
  void handleMenuCommand(int commandId);

  struct MenuCommand
  {
    int commandId;
    declara::app::MenuActionType action;
    declara::core::EmitterState *emitter;
  };

  struct MenuBinding
  {
    void *menuItem;
    State<bool> *enabledState;
  };

protected:
  virtual void applyMenuBar(Window *activeWindow);

private:
  void clearMenuBindings();
  int nextCommandId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  void *menuTarget_;
};

#endif // LOKA_MAC_APP_HPP
