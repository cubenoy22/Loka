#ifndef LOKA_MENU_CONTROLLER_HPP
#define LOKA_MENU_CONTROLLER_HPP

#include "app/Menu.hpp"
#include "core/util/OwnedDef.hpp"

class AppConfigurable;
class Window;

// Owns the provisional default/app menu composition state for App. Platform
// code still decides whether the resolved menu is applied application-wide or
// to a native window.
class MenuController
{
public:
  explicit MenuController(AppConfigurable *config);
  ~MenuController();

  void requestInvalidation();
  bool flushInvalidation();
  void invalidate();

  void setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar);
  const loka::app::MenuBarDefinition *defaultMenuBar() const;
  const loka::app::MenuBarDefinition *resolveMenuBar(Window *window);

  bool refreshDefaultMenuBar();
  const loka::app::MenuCompositionDiff &diff() const;
  void clearDiff();

private:
  static bool RefreshThunk(void *userData);

  AppConfigurable *config_;
  loka::core::OwnedDef<loka::app::MenuBarDefinition> menuBar_;
  loka::app::MenuCompositionDiff diff_;
};

#endif // LOKA_MENU_CONTROLLER_HPP
