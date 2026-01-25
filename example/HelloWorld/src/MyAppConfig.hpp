#ifndef LOKA_MY_APP_CONFIG_HPP
#define LOKA_MY_APP_CONFIG_HPP

#include "app/AppComposition.hpp"
#include "app/AppConfigurable.hpp"
#include "app/WindowDefinition.hpp"
#include "app/Menu.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx), menu_() {}

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(50, 50, 300, 240)
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
        : randomSeedState_(0), rebuildBound_(false), rebuildEvent_() {}

    virtual void composeMenu(loka::app::MenuComposition &c)
    {
      using namespace loka::app;
      c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
                          << MenuSeparator()
                          << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
      c.declare(Menu("View") << MenuItem("Color Picker").actionType(MENU_ACTION_SHOW_COLOR_PICKER));
      c.declare(Menu("File") << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
      c.declare(Menu("Special") << (MenuItem("Item") << MenuItem("Sub Item")) << MenuItem("Item 2"));
      if (!randomSeedState_)
      {
        randomSeedState_ = &c.useState<unsigned int>(0x1234);
      }
      if (!rebuildBound_)
      {
        rebuildEvent_.bind(&MainMenu::RebuildThunk, this, false);
        rebuildBound_ = true;
      }
      MenuDefinition randomMenu("Random");
      const unsigned int seed = randomSeedState_ ? randomSeedState_->get() : 0u;
      buildRandomMenu(randomMenu, seed);
      c.declare(randomMenu);
    }

  private:
    void buildRandomMenu(loka::app::MenuDefinition &menu, unsigned int seed)
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
        int j = static_cast<int>(seed % static_cast<unsigned int>(i + 1));
        const MenuItemDefinition tmp = labels[i];
        labels[i] = labels[j];
        labels[j] = tmp;
        seed = seed * 1103515245u + 12345u;
      }
      for (int i = 0; i < 6; ++i)
      {
        menu << labels[i];
      }
    }

    void handleRebuild()
    {
      if (randomSeedState_)
      {
        unsigned int seed = randomSeedState_->get();
        seed = seed * 1103515245u + 12345u;
        randomSeedState_->set(seed);
      }
    }

    static void RebuildThunk(void *userData)
    {
      MainMenu *self = static_cast<MainMenu *>(userData);
      if (self)
      {
        self->handleRebuild();
      }
    }

    loka::core::MutableState<unsigned int> *randomSeedState_;
    bool rebuildBound_;
    loka::core::EmitterState rebuildEvent_;
  };

  MainMenu menu_;
};

#endif // LOKA_MY_APP_CONFIG_HPP
