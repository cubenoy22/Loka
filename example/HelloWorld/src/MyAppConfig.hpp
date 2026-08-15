#ifndef LOKA_MY_APP_CONFIG_HPP
#define LOKA_MY_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/App.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/Menu.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  MyAppConfig(PlatformContext *ctx, unsigned long menuSeed)
      : AppConfigurable(ctx),
        menu_(menuSeed)
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
  private:
    class MenuRandom
    {
    public:
      explicit MenuRandom(unsigned long seed)
          : state_(seed)
      {
      }

      int nextIndex(int upperBound)
      {
        // Fixed C++98 arithmetic keeps a seed's menu sequence independent
        // of platform C-library rand() implementations and other app code.
        this->state_ =
            (this->state_ * 1664525UL + 1013904223UL) & 0xFFFFFFFFUL;
        return static_cast<int>((this->state_ >> 16) %
                                static_cast<unsigned long>(upperBound));
      }

    private:
      unsigned long state_;
    };

  public:
    explicit MainMenu(unsigned long seed)
        : rebuildBound_(false),
          rebuildEvent_(),
          random_(seed)
    {
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
        int j = this->random_.nextIndex(i + 1);
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
    MenuRandom random_;
  };

  MainMenu menu_;
};

#endif // LOKA_MY_APP_CONFIG_HPP
