#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "core/AppComposition.hpp"
#include "core/AppConfigurable.hpp"
#include "core/WindowDefinition.hpp"
#include "app/Empty.hpp"
#include "app/Menu.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx) : AppConfigurable(ctx) {}

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(40, 40, 320, 240)
                       .scene(declara::app::EmptyDefinition())
                       .title("LokaSimpleViewer")
                       .visible(true));
  }

  virtual void composeMenu(declara::app::MenuComposition &c)
  {
    using namespace declara::app;
    c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
                        << MenuSeparator()
                        << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
    c.declare(Menu("File") << MenuItem("Open..."));
  }
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
