#ifndef LOKA_APP_HPP
#define LOKA_APP_HPP

#include "core/ComponentGroup.hpp"
#include "core/AppComponent.hpp"
#include "core/AppConfigurable.hpp"
#include "app/Menu.hpp"
#include "loka/dsl/RefreshLoop.hpp"
#include <cassert>

class Window;
class AppComposition;

class App : public AppComponent
{
public:
  explicit App(AppConfigurable *config);
  virtual ~App();

  virtual void run();
  virtual void quit() = 0;
  virtual void windowClosed(Window *window);
  virtual bool handleMenuCommand(int commandId, Window *window);
  void invalidateMenu();
  void setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar);
  const loka::app::MenuBarDefinition *defaultMenuBar() const { return menuBar_; }
  void setActiveWindow(Window *window);
  Window *activeWindow() const { return activeWindow_; }

protected:
  ComponentGroup<AppComponent> *group_;
  bool quitWhenLastWindowClosed_;
  AppConfigurable *config_;
  loka::app::MenuBarDefinition *menuBar_;
  Window *activeWindow_;
  loka::dsl::RefreshLoop menuRefresh_;
  loka::app::MenuCompositionDiff menuDiff_;

  const loka::app::MenuBarDefinition *resolveMenuBar(Window *window);
  virtual void applyMenuBar(Window *activeWindow);
  bool refreshDefaultMenuBar();

  const loka::app::MenuCompositionDiff &menuDiff() const { return menuDiff_; }
  void clearMenuDiff();

  void reflectInitialVisibilityChunks();

private:
  static bool MenuRefreshThunk(void *userData);
  static void MenuApplyThunk(void *userData);
};

#endif // LOKA_APP_HPP
