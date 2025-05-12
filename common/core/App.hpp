#ifndef DECLARA_APP_HPP
#define DECLARA_APP_HPP

#include <vector>
#include "core/ComponentGroup.hpp"
#include "core/AppComponent.hpp"

class Window;

class App : public AppComponent
{
public:
  explicit App(ComponentGroup<AppComponent> *group);
  virtual ~App();

  virtual void run();
  virtual void quit() = 0;
  virtual void windowClosed(Window *window);

  // AppBuilderからAppComponent群を構築する仮想関数
  virtual ComponentGroup<AppComponent> *composeComponents(AppBuilder &builder)
  {
    return new ComponentGroup<AppComponent>(builder.build());
  }

protected:
  ComponentGroup<AppComponent> *group_;
  bool quitWhenLastWindowClosed_ = true;
};

#endif // DECLARA_APP_HPP
