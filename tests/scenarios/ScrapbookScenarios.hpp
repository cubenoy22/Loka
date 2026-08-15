#ifndef LOKA_TESTS_SCENARIOS_SCRAPBOOK_SCENARIOS_HPP
#define LOKA_TESTS_SCENARIOS_SCRAPBOOK_SCENARIOS_HPP

#include <string>

#include "testing/scene/ScenarioAudit.hpp"
#include "testing/snap/SnapFormat.hpp"
#include "ScenarioTypes.hpp"

#ifdef TEST_BUILD
#include "app/scene/state/FlowSlot.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#endif

namespace scrapbook
{
  class MainNode;
}

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Scene;
    }
  } // namespace app

  namespace scenario_tests
  {
    /** Immutable selection and terminal ownership for one scenario run. */
    class ScenarioLaunchPlan
    {
    public:
      ScenarioLaunchPlan();

#ifdef TEST_BUILD
      /** Builds the presentation-only tour compiled into TEST-only targets. */
      static ScenarioLaunchPlan StandaloneTour();
#endif

      bool isValid() const;
      const std::string &scenario() const;
      ScenarioCompletionPolicy completionPolicy() const;

    private:
      ScenarioLaunchPlan(const std::string &scenario, ScenarioCompletionPolicy completionPolicy);

      friend bool QueryRigLaunchPlan(bool,
                                     const dsl::SnapTestConfig::Settings &,
                                     ScenarioLaunchPlan &);

      bool valid_;
      std::string scenario_;
      ScenarioCompletionPolicy completionPolicy_;
    };

    /** Resolves a config-backed rig launch without allowing presentation-only
        scenarios to become machine-verdict runs. Refusal leaves out unchanged. */
    bool QueryRigLaunchPlan(bool configLoaded,
                            const dsl::SnapTestConfig::Settings &settings,
                            ScenarioLaunchPlan &out);

    /** Owns one step-driven scenario's observations between idle ticks. */
    class ScrapbookScenario
    {
    public:
      explicit ScrapbookScenario(const ScenarioLaunchPlan &plan,
                                 dsl::testing::ScenarioAuditSink *audit = 0);

      /** Drives one scenario step and names who owns the terminal state. */
      ScenarioAdvance
      step(long tick,
           app::scene::Scene *scene,
           scrapbook::MainNode &mainNode,
           const CaptureContentBounds &bounds,
           dsl::SnapRecord &out);

      /** Publishes the completed driver-owned verdict after rail-local checks. */
      bool publishVerdict(const dsl::SnapRecord &record);

      /** Returns the immutable scenario selection owned by this run. */
      const std::string &name() const;

#ifdef TEST_BUILD
      /** Records an orderly canceled terminal when the presentation owner
          stops while its Flow is still pending. Idempotent. */
      void stop();
#endif

    private:
      enum Kind
      {
        KIND_INVALID = 0,
        KIND_STARTUP,
        KIND_OPEN_FIRST_PAGE,
        KIND_OPEN_FIRST_PAGE_REFUSED,
        KIND_FLIP_FORWARD_BACK,
        KIND_REFUSED_FLIP_KEEPS_PAGE,
        KIND_OPEN_TEXT_PAGE,
        KIND_OPEN_TEXT_PAGE_REFUSED
#ifdef TEST_BUILD
        ,
        KIND_STANDALONE_TOUR
#endif
      };

      struct PageObservation
      {
        PageObservation()
            : published(false),
              page(-1),
              captionAvailable(false),
              caption()
        {
        }

        bool published;
        int page;
        bool captionAvailable;
        std::string caption;
      };

      bool runOpenScenario(long tick,
                           const scrapbook::MainNode &mainNode,
                           const CaptureContentBounds &bounds,
                           dsl::SnapRecord &out);
      bool
      runFlipForwardBack(long tick,
                         scrapbook::MainNode &mainNode,
                         const CaptureContentBounds &bounds,
                         dsl::SnapRecord &out);
      bool runRefusedFlipKeepsPage(long tick,
                                   scrapbook::MainNode &mainNode,
                                   const CaptureContentBounds &bounds,
                                   dsl::SnapRecord &out);
      bool
      runOpenTextPage(long tick,
                      scrapbook::MainNode &mainNode,
                      const CaptureContentBounds &bounds,
                      dsl::SnapRecord &out);
#ifdef TEST_BUILD
      typedef dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord> StandaloneTourFlowChain;

      /** Owns the scheduled tour and its borrowed input slot as one lifecycle. */
      class StandaloneTourState
      {
      public:
        StandaloneTourState();

        void start(dsl::testing::ScenarioAuditSink *audit);
        dsl::FlowRunResult run(long tick, app::scene::Scene *scene);
        void stop();
        const dsl::SnapRecord &record() const;

      private:
        dsl::testing::ScenarioClock clock_;
        app::scene::Scene *scene_;
        dsl::SnapRecord record_;
        app::scene::FlowSlot<StandaloneTourFlowChain> flow_;

        StandaloneTourState(const StandaloneTourState &);
        StandaloneTourState &operator=(const StandaloneTourState &);
      };
#endif

#ifdef TEST_BUILD
      bool runStandaloneTour(long tick,
                             app::scene::Scene *scene,
                             scrapbook::MainNode &mainNode,
                             const CaptureContentBounds &bounds,
                             dsl::SnapRecord &out);
#endif
      static PageObservation observePage(const scrapbook::MainNode &mainNode);
      static void setPageObservation(dsl::SnapRecord &record,
                                     const char *pageKey,
                                     const char *captionKey,
                                     const PageObservation &observation);

      ScenarioLaunchPlan plan_;
      Kind kind_;
      ScenarioAdvance terminalState_;
      int stage_;
      PageObservation step1_;
      PageObservation step2_;
      dsl::testing::scenario_audit_detail::TerminalEmitter terminalAudit_;
#ifdef TEST_BUILD
      StandaloneTourState standaloneTour_;
#endif

      ScrapbookScenario(const ScrapbookScenario &);
      ScrapbookScenario &operator=(const ScrapbookScenario &);
    };

    dsl::SnapRecord MakeDriverErrorRecord(const char *scenario, long errorCode, const char *message);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCRAPBOOK_SCENARIOS_HPP
