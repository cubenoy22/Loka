#ifndef LOKA_SMIRK_BENCH_APP_CONFIG_HPP
#define LOKA_SMIRK_BENCH_APP_CONFIG_HPP

#include "MainNode.hpp"
#include "SmirkModel.hpp"
#include "app/Menu.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"

class SmirkBenchAppConfig : public AppConfigurable
{
public:
  explicit SmirkBenchAppConfig(PlatformContext *context)
      : AppConfigurable(context),
        model_(640, 400)
  {
  }

  virtual void compose(AppComposition &composition)
  {
    composition << WindowDef(
        WindowProps()
            .frame(50, 50, 640, 400)
            .scene(loka::app::scene::Boundary<smirkbench::MainNode>(smirkbench::MainProps(&this->model_)))
            .title("LokaSmirkBench")
            .visible(true)
            .idlePolicy(loka::app::IdlePolicy::interval(smirkbench::kFixedStepSeconds))
            .onIdle(&SmirkBenchAppConfig::WindowIdleThunk, this));
  }

  virtual void composeMenu(loka::app::MenuComposition &composition)
  {
    using namespace loka::app;
    composition.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP) << MenuSeparator()
                                  << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
  }

private:
  static void WindowIdleThunk(Window *window, double elapsedSeconds, void *userData)
  {
    (void)window;
    SmirkBenchAppConfig *self = static_cast<SmirkBenchAppConfig *>(userData);
    if (self)
    {
      self->model_.advanceFrame(elapsedSeconds);
    }
  }

  smirkbench::SmirkModel model_;
};

#endif // LOKA_SMIRK_BENCH_APP_CONFIG_HPP
