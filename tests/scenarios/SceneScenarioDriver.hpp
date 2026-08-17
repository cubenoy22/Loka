#ifndef LOKA_TESTS_SCENARIOS_SCENE_SCENARIO_DRIVER_HPP
#define LOKA_TESTS_SCENARIOS_SCENE_SCENARIO_DRIVER_HPP

#include <string>

#include "ScenarioTypes.hpp"
#include "StartupScenarios.hpp"
#include "app/core/Window.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Platform-neutral adapter driven by a platform scenario run owner. */
    class ScenarioDriver
    {
    public:
      virtual ~ScenarioDriver() {}

      virtual ScenarioAdvance
      step(long tick, Window *window, const CaptureContentBounds &bounds, dsl::SnapRecord &out) = 0;
      virtual bool publishVerdict(const dsl::SnapRecord &record) = 0;
    };

    typedef dsl::SnapRecord (*DriverErrorFactory)(long errorCode, const char *message);

    /** Adapts one startup or interaction scenario to the shared platform
        capture and completion protocol. */
    template <class InteractionScenario> class SceneScenarioDriver : public ScenarioDriver
    {
    public:
      SceneScenarioDriver(bool startup,
                          StartupExample startupExample,
                          DriverErrorFactory errorFactory,
                          long interactionErrorCode,
                          dsl::testing::ScenarioAuditSink *audit)
          : startup_(startup),
            startupExample_(startupExample),
            errorFactory_(errorFactory),
            interactionErrorCode_(interactionErrorCode),
            startupScenario_(startupExample, SCENARIO_COMPLETION_DRIVER_OWNED, audit),
            scenario_(SCENARIO_COMPLETION_DRIVER_OWNED, audit)
      {
      }

      SceneScenarioDriver(bool startup,
                          StartupExample startupExample,
                          const std::string &interactionName,
                          DriverErrorFactory errorFactory,
                          long interactionErrorCode,
                          dsl::testing::ScenarioAuditSink *audit)
          : startup_(startup),
            startupExample_(startupExample),
            errorFactory_(errorFactory),
            interactionErrorCode_(interactionErrorCode),
            startupScenario_(startupExample, SCENARIO_COMPLETION_DRIVER_OWNED, audit),
            scenario_(interactionName, SCENARIO_COMPLETION_DRIVER_OWNED, audit)
      {
      }

      virtual ~SceneScenarioDriver()
      {
        if (this->startup_)
        {
          this->startupScenario_.stop();
        }
        else
        {
          this->scenario_.stop();
        }
      }

      virtual ScenarioAdvance
      step(long tick, Window *window, const CaptureContentBounds &bounds, dsl::SnapRecord &out)
      {
        if (!window || !window->scene())
        {
          out = this->startup_ ? MakeStartupDriverErrorRecord(this->startupExample_, 2802, "Scene was not mounted")
                               : this->errorFactory_(this->interactionErrorCode_, "Scene was not mounted");
          return SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
        }
        return this->startup_ ? this->startupScenario_.step(tick, window->scene(), bounds, out)
                              : this->scenario_.step(tick, window->scene(), bounds, out);
      }

      virtual bool publishVerdict(const dsl::SnapRecord &record)
      {
        return this->startup_ ? this->startupScenario_.publishVerdict(record) : this->scenario_.publishVerdict(record);
      }

    private:
      const bool startup_;
      const StartupExample startupExample_;
      DriverErrorFactory errorFactory_;
      const long interactionErrorCode_;
      StartupScenario startupScenario_;
      InteractionScenario scenario_;

      SceneScenarioDriver(const SceneScenarioDriver &);
      SceneScenarioDriver &operator=(const SceneScenarioDriver &);
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENE_SCENARIO_DRIVER_HPP
