#ifndef LOKA_MY_APP_CONFIG_HPP
#define LOKA_MY_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/App.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/Menu.hpp"
#include "MainNode.hpp"

#include <cstdlib>
#include <ctime>

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx),
        menu_()
  {
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(50, 50, 420, 300)
                       .scene(loka::app::scene::NodeDefinition<helloworld::MainProps, helloworld::MainNode>())
                       .title("LokaSample")
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
    MainMenu()
        : rebuildBound_(false),
          rebuildEvent_()
    {
      std::srand(static_cast<unsigned int>(std::time(0)));
    }

    virtual void composeMenu(loka::app::MenuComposition &c)
    {
      using namespace loka::app;
      c.declare(AppMenu()                                              //
                << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP) //
                << MenuSeparator()                                     //
                << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
      c.declare(Menu("View") //
                << MenuItem("Color Picker").actionType(MENU_ACTION_SHOW_COLOR_PICKER));
      c.declare(Menu("File") //
                << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
      c.declare(Menu("Special")                               //
                << (MenuItem("Item") << MenuItem("Sub Item")) //
                << MenuItem("Item 2"));
      if (!rebuildBound_)
      {
        this->bindActionForMenu(this->rebuildEvent_, &MainMenu::handleRebuild);
        rebuildBound_ = true;
      }
      MenuDefinition randomMenu("Random");
      buildRandomMenu(randomMenu);
      c.declare(randomMenu);
    }

  private:
    void buildRandomMenu(loka::app::MenuDefinition &menu)
    {
      using namespace loka::app;
      using namespace loka::core;

      MenuItemDefinition labels[6];
      for (int i = 0; i < 6; ++i)
      {
        labels[i] = MenuItem(String::Literal("Random ") + String::FromInt(i + 1));
      }
      menu.opaqueChildren(false);
      menu << MenuItem("Rebuild menu").actionType(MENU_ACTION_REBUILD_MENU).onClick(&rebuildEvent_);
      menu << MenuSeparator();
      for (int i = 5; i > 0; --i)
      {
        int j = std::rand() % (i + 1);
        const MenuItemDefinition tmp = labels[i];
        labels[i] = labels[j];
        labels[j] = tmp;
      }
      for (int i = 0; i < 6; ++i)
      {
        menu << labels[i];
      }
    }

    void handleRebuild() {}

    bool rebuildBound_;
    loka::core::EmitterState rebuildEvent_;
  };

  MainMenu menu_;
};

#endif // LOKA_MY_APP_CONFIG_HPP
