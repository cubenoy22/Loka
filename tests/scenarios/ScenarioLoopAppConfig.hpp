#ifndef LOKA_TESTS_SCENARIOS_SCENARIO_LOOP_APP_CONFIG_HPP
#define LOKA_TESTS_SCENARIOS_SCENARIO_LOOP_APP_CONFIG_HPP

#include "ScenarioCellTable.hpp"
#include "ScenarioReel.hpp"
#include "StartupScenarios.hpp"
#include "app/core/App.hpp"
#include "app/core/Window.hpp"

/** How long a settled cell stays on screen before the reel re-arms. */
#ifndef LOKA_SCENARIO_LOOP_HOLD_SECONDS
#define LOKA_SCENARIO_LOOP_HOLD_SECONDS 3.0
#endif

/** Full passes to run before quitting. 0, the default a shipped loop build
    carries, never quits. A positive value is the verification knob: it lets a
    MAME or host run end on its own and be asserted, and it changes nothing
    about how a cell is stepped or re-armed -- only when the reel retires. */
#ifndef LOKA_SCENARIO_LOOP_CYCLES
#define LOKA_SCENARIO_LOOP_CYCLES 0
#endif

namespace loka
{
  namespace scenario_tests
  {
    /** Binds one example's scenario presentation to a reel.

        The presentation already declares the example's production window, so
        this adds exactly two things: the idle pump that drives the reel, and
        the App a bounded run has to quit when it retires. Both examples need
        the same rule for when to quit, which is why it lives here once rather
        than in each vehicle. */
    template <class PresentationT, class InteractionScenarioT>
    class ScenarioLoopAppConfig : public PresentationT
    {
    public:
      /** The presentation argument is a template parameter only because each
          example seeds its presentation with its own type (a MainProps, a menu
          seed); everything after it is the shared reel wiring. */
      template <class PresentationArgT>
      ScenarioLoopAppConfig(PlatformContext *context,
                            const PresentationArgT &presentationArg,
                            const ScenarioCellTable &cells,
                            StartupExample startupExample,
                            DriverErrorFactory errorFactory,
                            long interactionErrorCode,
                            double holdSeconds,
                            long cycleBudget)
          : PresentationT(context, presentationArg),
            reel_(cells, startupExample, errorFactory, interactionErrorCode, holdSeconds, cycleBudget),
            borrowedApp_(0)
      {
        this->presentScenarioReelTitle(this->reel_.operatorTitlePublisher());
      }

      void setApp(App *app)
      {
        this->borrowedApp_ = app;
      }

    protected:
      virtual void onScenarioIdle(Window *window, double elapsedSeconds)
      {
        this->reel_.tick(window, elapsedSeconds, window ? window->getTracker() : 0);
        if (this->reel_.finished() && this->borrowedApp_)
        {
          this->borrowedApp_->quit();
        }
      }

    private:
      ScenarioReel<InteractionScenarioT> reel_;
      App *borrowedApp_;

      ScenarioLoopAppConfig(const ScenarioLoopAppConfig &);
      ScenarioLoopAppConfig &operator=(const ScenarioLoopAppConfig &);
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENARIO_LOOP_APP_CONFIG_HPP
