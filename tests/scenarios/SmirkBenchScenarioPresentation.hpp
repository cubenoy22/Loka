#ifndef LOKA_TESTS_SMIRK_BENCH_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SMIRK_BENCH_SCENARIO_PRESENTATION_HPP

#include "../../example/SmirkBench/src/MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"
#include "SmirkBenchEditorLineNode.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Deterministic desktop presentation: production scene and menu, frozen
        surface model. Like the Toolbox vehicle, only the driver advances it. */
    class SmirkBenchScenarioPresentation : public SmirkBenchAppConfig
    {
    public:
      SmirkBenchScenarioPresentation(PlatformContext *context, bool specimen)
          : SmirkBenchAppConfig(context),
            specimen_(specimen)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        WindowProps window;
        if (this->specimen_)
        {
          window.scene(ObservedMainDefinition<SmirkBenchEditorLineProps, SmirkBenchEditorLineNode>(
              SmirkBenchEditorLineProps(&this->model()), 0));
        }
        else
        {
          window.scene(ObservedMainDefinition<smirkbench::MainProps, smirkbench::MainNode>(
              smirkbench::MainProps(&this->model()), 0));
        }
        composition << WindowDef(window
                                     .frame(50, 50, 640, 400)
                                     .title("LokaSmirkBench")
                                     .visible(true)
                                     .idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&SmirkBenchScenarioPresentation::OnIdle, this));
      }

    protected:
      virtual void onScenarioIdle(Window *window, double elapsedSeconds) = 0;

    private:
      static void OnIdle(Window *window, double elapsedSeconds, void *owner)
      {
        static_cast<SmirkBenchScenarioPresentation *>(owner)->onScenarioIdle(window, elapsedSeconds);
      }
      const bool specimen_;
    };
  } // namespace scenario_tests
} // namespace loka
#endif
