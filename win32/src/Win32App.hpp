#ifndef LOKA_WIN32APP_HPP
#define LOKA_WIN32APP_HPP

#include "app/core/App.hpp"
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
  bool handleMenuCommand(int commandId, Window *window);

protected:
  virtual void applyMenuBar(Window *activeWindow);

private:
  /** Owns one menu while recording the native window to which it is attached.
      Reset always detaches the menu before destroying its handle. */
  class AttachedMenu
  {
  public:
    AttachedMenu();
    ~AttachedMenu();

    bool resetPreservingContentFrame();
    bool resetForTeardown();
    void attach(Win32Window *window, HMENU menu);

  private:
    enum DetachMode
    {
      DETACH_PRESERVING_CONTENT_FRAME,
      DETACH_FOR_TEARDOWN
    };

    AttachedMenu(const AttachedMenu &);
    AttachedMenu &operator=(const AttachedMenu &);

    bool reset(DetachMode mode);
    bool detach(DetachMode mode);
    void destroyDetached();

    Win32Window *window_;
    HMENU menu_;
  };

  struct MenuCommand
  {
    int commandId;
    loka::app::MenuActionType action;
    loka::core::EmitterState *emitter;
  };

  struct MenuBinding
  {
    HMENU menu;
    UINT commandId;
    HWND hwnd;
    loka::core::State<bool> *enabledState;
    bool invertEnabled;
  };

  void clearMenuBindings();
  static void MenuEnabledChangedThunk(void *userData);
  void buildMenuItem(HMENU menu, const loka::app::MenuItemDefinition *itemDef, HWND hwnd);
  void buildMenuItems(HMENU menu, const loka::app::MenuItemDefinition *itemsHead, HWND hwnd);

  HINSTANCE hInstance_;
  int nCmdShow_;
  int nextCommandId_;
  AttachedMenu activeMenu_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
};

#endif // LOKA_WIN32APP_HPP
