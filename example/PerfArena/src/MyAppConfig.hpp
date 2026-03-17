#ifndef LOKA_PERF_ARENA_APP_CONFIG_HPP
#define LOKA_PERF_ARENA_APP_CONFIG_HPP

#include "app/AppComposition.hpp"
#include "app/AppConfigurable.hpp"
#include "app/Menu.hpp"
#include "app/WindowDefinition.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx), menu_(), shared_()
  {
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(60, 60, 1120, 760)
                       .scene(perfarena::ArenaShellBoundary(perfarena::ArenaShellProps(&this->shared_)))
                       .titleState(&this->shared_.titleText_)
                       .visible(true));
  }

  virtual void composeMenu(loka::app::MenuComposition &c)
  {
    c << menu_;
  }

private:
  class MainMenu : public loka::app::MenuBoundary
  {
  public:
    virtual void composeMenu(loka::app::MenuComposition &c)
    {
      using namespace loka::app;
      c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
                          << MenuSeparator()
                          << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
      c.declare(Menu("File") << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
    }
  };

  MainMenu menu_;
  perfarena::SharedModel shared_;
};

#endif // LOKA_PERF_ARENA_APP_CONFIG_HPP
