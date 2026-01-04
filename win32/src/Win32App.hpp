#ifndef LOKA_WIN32APP_HPP
#define LOKA_WIN32APP_HPP

#include "core/App.hpp"
#include <windows.h>
#include <vector>

class Win32Window;

class Win32App : public App
{
protected:
  Win32App(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow);
  virtual ~Win32App();
  friend class Win32PlatformContext;

public:
  virtual void run();
  virtual void quit();
  virtual void windowClosed(Window *window);
  bool handleMenuCommand(int commandId, Window *window);

protected:
  virtual void applyMenuBar(Window *activeWindow);

private:
  struct MenuCommand
  {
    int commandId;
    declara::app::MenuActionType action;
    declara::core::EmitterState *emitter;
  };

  struct MenuBinding
  {
    HMENU menu;
    UINT commandId;
    HWND hwnd;
    State<bool> *enabledState;
  };

  void clearMenuBindings();
  static void MenuEnabledChangedThunk(void *userData);
  void buildMenuItems(HMENU menu, const std::vector<declara::app::MenuItemDefinition *> &items, HWND hwnd);

  HINSTANCE hInstance_;
  int nCmdShow_;
  int nextCommandId_;
  HMENU activeMenu_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
};

#endif // LOKA_WIN32APP_HPP
