#ifndef LOKA_MY_APP_CONFIG_HPP
#define LOKA_MY_APP_CONFIG_HPP

#include "core/AppComposition.hpp"
#include "core/AppConfigurable.hpp"
#include "core/WindowDefinition.hpp"
#include "app/Menu.hpp"
#include "MainComponent.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx) {}

  virtual void compose(AppComposition &c)
  {
    c.declare(WindowDef(WindowProps()
                            .scene(helloworld::Main())
                            .title("LokaSample")
                            .visible(true)));
  }

  virtual void composeMenu(declara::app::MenuComposition &c)
  {
    using namespace declara::app;
    c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
                        << MenuSeparator()
                        << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
    c.declare(Menu("View") << MenuItem("Color Picker").actionType(MENU_ACTION_SHOW_COLOR_PICKER));
    c.declare(Menu("File") << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
  }
};

#endif // LOKA_MY_APP_CONFIG_HPP
