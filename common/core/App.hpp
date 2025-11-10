#ifndef DECLARA_APP_HPP
#define DECLARA_APP_HPP

#include <vector>
#include "core/ComponentGroup.hpp"
#include "core/AppComponent.hpp"
#include "core/AppConfigurable.hpp"
#include <cassert>

class Window;
class AppBuilder;

class App : public AppComponent
{
public:
  explicit App(AppConfigurable *config);
  virtual ~App();

  virtual void run();
  virtual void quit() = 0;
  virtual void windowClosed(Window *window);

protected:
  ComponentGroup<AppComponent> *group_;
  bool quitWhenLastWindowClosed_;
  AppConfigurable *config_;

  void reflectInitialVisibilityChunks();
};

#endif // DECLARA_APP_HPP
