#ifndef LOKA_MENU_CONTROLLER_HPP
#define LOKA_MENU_CONTROLLER_HPP

#include "app/Menu.hpp"
#include "core/scheduler/NextTickTracker.hpp"

class AppConfigurable;
class Window;

// Owns the provisional default/app menu composition state for App. Platform
// code still decides whether the resolved menu is applied application-wide or
// to a native window.
class MenuController
{
public:
  typedef void (*ApplyFn)(void *userData, Window *activeWindow);

  MenuController(AppConfigurable *config, ApplyFn applyFn, void *applyUserData);
  ~MenuController();

  void requestInvalidation();
  bool flushInvalidation(Window *activeWindow);
  void invalidate(Window *activeWindow);

  void setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar, Window *activeWindow);
  const loka::app::MenuBarDefinition *defaultMenuBar() const;
  const loka::app::MenuBarDefinition *resolveMenuBar(Window *window);

  bool refreshDefaultMenuBar();
  const loka::app::MenuCompositionDiff &diff() const;
  void clearDiff();

private:
  static void InvalidateThunk(void *userData);
  static bool RefreshThunk(void *userData);
  static void ApplyThunk(void *userData);

  void apply(Window *activeWindow);

  AppConfigurable *config_;
  ApplyFn applyFn_;
  void *applyUserData_;
  Window *pendingApplyWindow_;
  loka::app::MenuBarDefinition *menuBar_;
  loka::core::NextTickTracker refresh_;
  loka::app::MenuCompositionDiff diff_;
};

#endif // LOKA_MENU_CONTROLLER_HPP
