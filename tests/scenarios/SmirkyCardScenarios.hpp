#ifndef LOKA_TESTS_SCENARIOS_SMIRKY_CARD_SCENARIOS_HPP
#define LOKA_TESTS_SCENARIOS_SMIRKY_CARD_SCENARIOS_HPP

#include <string>

#include "ScenarioCellTable.hpp"
#include "ScenarioTypes.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace app { namespace scene { class Scene; } }
  namespace scenario_tests
  {
    ScenarioCellTable SmirkyCardReelCells();
    bool IsSmirkyCardScenario(const std::string &name);

    /** Drives SmirkyCard's rendered Button binding, then observes the Scene
        admitted by the App on its next normal pass. */
    class SmirkyCardScenario
    {
    public:
      SmirkyCardScenario(const std::string &name, ScenarioCompletionPolicy completionPolicy,
                          dsl::testing::ScenarioAuditSink *audit = 0);
      ScenarioAdvance step(long tick, app::scene::Scene *scene,
                           const CaptureContentBounds &bounds, dsl::SnapRecord &out);
      bool publishVerdict(const dsl::SnapRecord &record);
      void stop();

    private:
      const std::string name_;
      const ScenarioCompletionPolicy completionPolicy_;
      ScenarioAdvance terminalState_;
      bool emitted_;
      dsl::testing::scenario_audit_detail::TerminalEmitter terminalAudit_;
    };

    dsl::SnapRecord MakeSmirkyCardDriverErrorRecord(long errorCode, const char *message);
  }
}

#endif
