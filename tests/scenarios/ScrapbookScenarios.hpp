#ifndef LOKA_TESTS_SCENARIOS_SCRAPBOOK_SCENARIOS_HPP
#define LOKA_TESTS_SCENARIOS_SCRAPBOOK_SCENARIOS_HPP

#include <string>

#include "testing/snap/SnapFormat.hpp"

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
    enum ScenarioCompletionPolicy
    {
      SCENARIO_COMPLETION_DRIVER_OWNED = 0,
      SCENARIO_COMPLETION_HOLD_FINAL_SCENE
    };

    enum ScenarioAdvance
    {
      SCENARIO_ADVANCE_PENDING = 0,
      SCENARIO_ADVANCE_DRIVER_COMPLETION_READY,
      SCENARIO_ADVANCE_FINAL_SCENE_HELD
    };

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

    /** Half-open content rectangle in the captured content's local coordinates. */
    struct CaptureContentBounds
    {
      CaptureContentBounds()
          : available(false),
            left(0),
            top(0),
            right(0),
            bottom(0)
      {
      }

      bool available;
      long left;
      long top;
      long right;
      long bottom;
    };

    /** Owns one step-driven scenario's observations between idle ticks. */
    class ScrapbookScenario
    {
    public:
      explicit ScrapbookScenario(const ScenarioLaunchPlan &plan
#ifdef TEST_BUILD
                                 , dsl::testing::ScenarioAuditSink *audit = 0
#endif
      );

      /** Drives one scenario step and names who owns the terminal state. */
      ScenarioAdvance
      step(long tick,
           app::scene::Scene *scene,
           scrapbook::MainNode &mainNode,
           const CaptureContentBounds &bounds,
           dsl::SnapRecord &out);

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
        explicit StandaloneTourState(dsl::testing::ScenarioAuditSink *audit);

        void start(dsl::testing::ScenarioAuditSink *audit);
        dsl::FlowRunResult run(long tick, app::scene::Scene *scene);
        bool finish(dsl::testing::ScenarioAuditTerminalStatus status);
        void stop();
        const dsl::SnapRecord &record() const;

      private:
        dsl::testing::ScenarioClock clock_;
        app::scene::Scene *scene_;
        dsl::SnapRecord record_;
        dsl::testing::scenario_audit_detail::TerminalEmitter terminalAudit_;
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
