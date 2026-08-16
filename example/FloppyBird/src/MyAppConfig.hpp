#ifndef LOKA_FLOPPY_BIRD_APP_CONFIG_HPP
#define LOKA_FLOPPY_BIRD_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/Menu.hpp"
#include "app/RectSurface.hpp"
#include "app/core/WindowDefinition.hpp"
#include "GameModel.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx),
        game_(1UL)
  {
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(50, 50, 380, 340)
                       .idlePolicy(loka::app::IdlePolicy::interval(loka_floppy_bird::kFixedStepSeconds))
                       .onIdle(&MyAppConfig::WindowIdleThunk, this)
                       .onKeyPress(&MyAppConfig::WindowKeyPressThunk, this)
                       .scene(loka::app::scene::NodeDefinition<floppybird::MainProps, floppybird::MainNode>(
                           floppybird::MainProps(&this->game_)))
                       .title("LokaFloppyBird")
                       .visible(true));
  }

  virtual void composeMenu(loka::app::MenuComposition &c)
  {
    using namespace loka::app;
    c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP) << MenuSeparator()
                        << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
  }

  void handleWindowIdle(Window *window, double elapsedSeconds)
  {
    (void)window;
    this->game_.advanceFrame(elapsedSeconds);
  }

  bool handleWindowKeyPress(Window *window, char key)
  {
    (void)window;
    if (key != ' ')
    {
      return false;
    }
    this->game_.flap();
    return true;
  }

private:
  static void WindowIdleThunk(Window *window, double elapsedSeconds, void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->handleWindowIdle(window, elapsedSeconds);
    }
  }

  static bool WindowKeyPressThunk(Window *window, char key, void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    return self ? self->handleWindowKeyPress(window, key) : false;
  }

  floppybird::GameModel game_;
};

#endif
