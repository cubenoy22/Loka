#ifndef LOKA_APP_HPP
#define LOKA_APP_HPP

#include "app/core/AppComponentGroup.hpp"
#include "app/core/AppComponent.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/MenuController.hpp"
#include "app/Menu.hpp"
#include <cassert>

class Window;
class AppComposition;

// App is owned by the platform/application layer. Code that needs an App
// instance should reach it through an owner-side path such as Window, not
// through a global current-App accessor.
class App : public AppComponent
{
public:
  explicit App(AppConfigurable *config);
  virtual ~App();

  virtual void run();
  virtual void quit() = 0;
  virtual void windowClosed(Window *window);
  virtual bool handleMenuCommand(int commandId, Window *window);
  loka::app::IdlePolicy idlePolicy() const;
  bool consumeIdle(double elapsedSeconds, double &dispatchElapsedSeconds);
  void handleIdle(double elapsedSeconds);
  bool handleKeyPress(char key);
  void requestMenuInvalidation();
  bool flushMenuInvalidation();
  void invalidateMenu();
  void setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar);
  const loka::app::MenuBarDefinition *defaultMenuBar() const
  {
    return menuController_.defaultMenuBar();
  }
  void setActiveWindow(Window *window);
  Window *activeWindow() const
  {
    return activeWindow_;
  }

protected:
  AppComponentGroup *group_;
  bool quitWhenLastWindowClosed_;
  AppConfigurable *config_;
  MenuController menuController_;
  Window *activeWindow_;
  double idleAccumulatedSeconds_;

  const loka::app::MenuBarDefinition *resolveMenuBar(Window *window);
  virtual void applyMenuBar(Window *activeWindow);
  bool refreshDefaultMenuBar();

  const loka::app::MenuCompositionDiff &menuDiff() const
  {
    return menuController_.diff();
  }
  void clearMenuDiff();

  void reflectInitialVisibilityChunks();
  void flushWindowInvalidations();

private:
  static void ApplyMenuBarThunk(void *userData, Window *activeWindow);
};

#endif // LOKA_APP_HPP
