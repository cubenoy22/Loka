#include "ScenarioDriver.hpp"

#include <cassert>

#include "ScrapbookScenarios.hpp"
#include "ScrapbookClassicScenarioPresentation.hpp"
#include "ScenarioDriverSupport.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/nodes/Text.hpp"
#include "context/ToolboxProjectedNodeContext.hpp"
#include "dsl/composition/CompositionList.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";

      struct BoundaryAuditCounts
      {
        int tagged;
        int untagged;
        BoundaryAuditCounts()
            : tagged(0),
              untagged(0)
        {
        }
      };

      /** Discriminating observer for the wall-side boundary hookup
          (ToolboxScenePlatformController::prepareProjectedLayout): every
          projected text context under a boundary must carry that boundary,
          or hit/text-run ledger entries would be attributed to the wrong
          window. Text is the representative projected kind; the hookup is
          one kind-agnostic line, so one kind discriminates its removal. */
      void AuditProjectedTextBoundaries(app::scene::Node *node,
                                        app::scene::BoundaryNode *expected,
                                        BoundaryAuditCounts &counts)
      {
        if (!node)
        {
          return;
        }
        if (app::TextNode *text = node->asTextNode())
        {
          if (app::scene::NodeContext *raw = text->getContext())
          {
            app::scene::IBoundaryTaggedContext *projected = raw->asBoundaryTagged();
            if (projected && projected->boundary() == expected)
            {
              ++counts.tagged;
            }
            else
            {
              ++counts.untagged;
            }
          }
        }
        if (app::scene::BoundaryNode *boundary = node->asBoundary())
        {
          AuditProjectedTextBoundaries(boundary->compositionRootNode(), boundary, counts);
          return;
        }
        if (app::scene::INestable *nestable = node->asNestable())
        {
          dsl::CompositionCursor<app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
          for (app::scene::Node *child = it.next(); child; child = it.next())
          {
            AuditProjectedTextBoundaries(child, expected, counts);
          }
        }
      }

      class ScenarioAppConfig : public scenario_tests::ScrapbookClassicScenarioPresentation
      {
      public:
        ScenarioAppConfig(PlatformContext *context,
                          const dsl::SnapTestConfig::Settings &settings,
                          const scenario_tests::ScenarioLaunchPlan &launchPlan)
            : scenario_tests::ScrapbookClassicScenarioPresentation(context),
              audit_(ResolveScenarioAuditFile(), launchPlan.scenario().c_str()),
              scenario_(launchPlan, &this->audit_),
              borrowedApp_(0),
              recorded_(false),
              tickCount_(0),
              lingerRemaining_(settings.hasLingerSeconds ? static_cast<double>(settings.lingerSeconds) : 0.0)
        {
        }

        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }

      private:
        virtual void onScenarioIdle(Window *window, double elapsedSeconds)
        {
          this->tick(window, elapsedSeconds);
        }

        void tick(Window *window, double elapsedSeconds)
        {
          ++this->tickCount_;
          if (!this->recorded_)
          {
            dsl::SnapRecord record;
            bool done = false;
            if (!this->observedMainNode())
            {
              record = scenario_tests::MakeDriverErrorRecord(
                  this->scenario_.name().c_str(), 2303, "MainNode was not mounted");
              done = true;
            }
            else
            {
              const scenario_tests::CaptureContentBounds captureBounds = QueryCaptureContentBounds(window);
              const scenario_tests::ScenarioAdvance advance = this->scenario_.step(this->tickCount_,
                                                                                   window ? window->scene() : 0,
                                                                                   *this->observedMainNode(),
                                                                                   ContentLocalBounds(captureBounds),
                                                                                   record);
              switch (advance)
              {
              case scenario_tests::SCENARIO_ADVANCE_PENDING:
                break;
              case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
                done = true;
                break;
              case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
                return;
              }
            }
            if (done && this->observedMainNode())
            {
              BoundaryAuditCounts counts;
              AuditProjectedTextBoundaries(this->observedMainNode(), 0, counts);
              if (counts.untagged != 0 || counts.tagged == 0)
              {
                record = scenario_tests::MakeDriverErrorRecord(
                    this->scenario_.name().c_str(), 2304, "projected text context lost its boundary tag");
              }
            }
            if (done)
            {
              (void)this->scenario_.publishVerdict(record);
              this->recorded_ = true;
              (void)this->hostCompletionSignal_.publish();
            }
          }
          if (!this->recorded_)
          {
            return;
          }
          // The record is already on disk, but an emulator-side snapshot
          // arrives on emulated wall-clock time; linger_seconds keeps the
          // scene on screen until then. Zero (the default) quits at once.
          this->lingerRemaining_ -= elapsedSeconds;
          if (this->lingerRemaining_ <= 0.0 && this->borrowedApp_)
          {
            this->borrowedApp_->quit();
          }
        }

        dsl::testing::ScenarioAuditFile audit_;
        scenario_tests::ScrapbookScenario scenario_;
        App *borrowedApp_;
        bool recorded_;
        long tickCount_;
        double lingerRemaining_;
        HostCompletionSignal hostCompletionSignal_;
      };
    } // namespace

    __attribute__((noinline)) void ScenarioTeardownComplete()
    {
      // noinline plus a store the optimizer must keep: -Os would otherwise
      // inline the call and drop the symbol the debugger breaks on.
      static volatile long teardownCompleteBeacon = 0;
      teardownCompleteBeacon = teardownCompleteBeacon + 1;
    }

    int RunScenarioApplication()
    {
      dsl::SnapTestConfig::Settings settings;
      const bool configLoaded = dsl::SnapTestConfig::load(kConfigPath, settings);
      if (!configLoaded)
      {
        (void)WriteScenarioErrorAudit(
            "startup", scenario_tests::MakeDriverErrorRecord("startup", 2300, "LokaTest.cfg is missing or invalid"));
        return 0;
      }
      if (!settings.hasScenario)
      {
        (void)WriteScenarioErrorAudit(
            "startup", scenario_tests::MakeDriverErrorRecord("startup", 2301, "scenario is missing"));
        return 0;
      }
      scenario_tests::ScenarioLaunchPlan launchPlan;
      if (!scenario_tests::QueryRigLaunchPlan(configLoaded, settings, launchPlan))
      {
        (void)WriteScenarioErrorAudit(
            settings.scenario.c_str(),
            scenario_tests::MakeDriverErrorRecord(settings.scenario.c_str(), 2302, "scenario is not registered"));
        return 0;
      }

      platform::InitPlatformRuntime();
      {
        core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
        assert(platformContext.get() && "PlatformContext is required");
        ScenarioAppConfig config(platformContext.get(), settings, launchPlan);
        core::ScopedPtr<App> app(platformContext->createApp(&config, 0, 0));
        assert(app.get() && "App is required");
        config.setApp(app.get());
        app->run();
      }
      // The App and PlatformContext are destroyed above, so a debugger break
      // planted here is the definitive teardown-complete marker: the
      // watchpoint leg (run-wpset.sh) keeps its freed-memory watches armed
      // until this function is reached rather than counting stack frames.
      ScenarioTeardownComplete();
      return 0;
    }
  } // namespace toolbox_tests
} // namespace loka
