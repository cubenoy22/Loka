#ifndef LOKA_TOOLBOX_APP_HPP
#define LOKA_TOOLBOX_APP_HPP

#include "app/core/App.hpp"
#include <vector>
#include <Menus.h>
#include "ToolboxActivationPhase.hpp"
class ToolboxApp : public App
{
protected:
  explicit ToolboxApp(AppConfigurable *config);
  virtual ~ToolboxApp();
  friend class ToolboxPlatformContext;

public:
  virtual void run();
  virtual void quit();
  void handleMenuSelection(short menuId, short item);
  static void MenuEnabledChangedThunk(void *userData);
  static void MenuCheckedChangedThunk(void *userData);

public:
  virtual void applyMenuBar(Window *activeWindow);

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
    loka::core::State<bool> *enabledState;
    bool invertEnabled;
    loka::core::State<bool> *checkedState;
  };

  /** Called by a live menu binding when it has updated the app-owned menu
      data. Foreground: redraws the menu bar immediately. Background: the
      shared menu bar is the foreground application's surface, so the single
      redraw is deferred until resume. */
  void noteMenuBarChangedFromBinding();

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
  void disposeHierarchicalMenus();
  /** Applies recorded scene changes and, while foreground, paints each window
      once at the run-loop tick's presentation boundary. */
  void present(ActivationPhase phase);
  /** The run loop owns this; every step branches on it rather than taking
      per-step booleans (see ToolboxActivationPhase.hpp). */
  ActivationPhase activationPhase_;
  /** A background binding changed the menu data; one DrawMenuBar is owed at
      resume. */
  bool menuBarDrawDeferred_;
  short nextMenuId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  std::vector<MenuEntry> menuEntries_;
  std::vector<MenuHandle> hierarchicalMenus_;
  bool running_;
};

#endif // LOKA_TOOLBOX_APP_HPP
