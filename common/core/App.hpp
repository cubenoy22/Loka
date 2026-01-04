#ifndef LOKA_APP_HPP
#define LOKA_APP_HPP

#include "core/ComponentGroup.hpp"
#include "core/AppComponent.hpp"
#include "core/AppConfigurable.hpp"
#include "app/Menu.hpp"
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
  void setDefaultMenuBar(const declara::app::MenuBarDefinition *menuBar);
  const declara::app::MenuBarDefinition *defaultMenuBar() const { return menuBar_; }
  void setActiveWindow(Window *window);
  Window *activeWindow() const { return activeWindow_; }

protected:
  ComponentGroup<AppComponent> *group_;
  bool quitWhenLastWindowClosed_;
  AppConfigurable *config_;
  declara::app::MenuBarDefinition *menuBar_;
  Window *activeWindow_;

  const declara::app::MenuBarDefinition *resolveMenuBar(Window *window) const;
  virtual void applyMenuBar(Window *activeWindow);

  void reflectInitialVisibilityChunks();
};

#endif // LOKA_APP_HPP
