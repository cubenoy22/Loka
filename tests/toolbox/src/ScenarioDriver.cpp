#include "ScenarioDriver.hpp"

#include <cassert>

#include <Quickdraw.h>

#include "MainNode.hpp"
#include "ScrapbookScenarios.hpp"
#include "ToolboxWindow.hpp"
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

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";
      const char *kCaptureFile = "LokaTestsToolbox.snap";
      const char *kCaptureMetadataFile = "LokaTestsToolbox.capture.snap";

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

      typedef app::scene::BoundaryDefinition<scrapbook::MainProps, scrapbook::MainNode> MainDefinitionBase;

      class ObservedMainDefinition : public MainDefinitionBase
      {
      public:
        ObservedMainDefinition(const scrapbook::MainProps &props, scrapbook::MainNode **observed)
            : MainDefinitionBase(props),
              observed_(observed)
        {
        }

        virtual app::scene::NodeDefinitionBase *clone() const
        {
          return new ObservedMainDefinition(*this);
        }

        virtual app::scene::Node *create() const
        {
          app::scene::Node *node = MainDefinitionBase::create();
          if (this->observed_)
          {
            // The scenario config borrows this node only while App::run owns
            // the window and its Scene; it records before asking App to quit.
            *this->observed_ = node ? static_cast<scrapbook::MainNode *>(node) : 0;
          }
          return node;
        }

      private:
        scrapbook::MainNode **observed_;
      };

      scenario_tests::CaptureContentBounds QueryContentBounds(Window *window)
      {
        scenario_tests::CaptureContentBounds result;
        ToolboxWindow *toolboxWindow = window ? window->asToolboxWindow() : 0;
        WindowPtr nativeWindow = toolboxWindow ? toolboxWindow->window() : 0;
        if (!nativeWindow)
        {
          return result;
        }

        Rect globalBounds = nativeWindow->portRect;
        GrafPtr previousPort = 0;
        GetPort(&previousPort);
        SetPort(nativeWindow);
        LocalToGlobal(reinterpret_cast<Point *>(&globalBounds.top));
        LocalToGlobal(reinterpret_cast<Point *>(&globalBounds.bottom));
        SetPort(previousPort);

        result.available = true;
        result.left = globalBounds.left;
        result.top = globalBounds.top;
        result.right = globalBounds.right;
        result.bottom = globalBounds.bottom;
        return result;
      }

      scenario_tests::CaptureContentBounds
      ContentLocalBounds(const scenario_tests::CaptureContentBounds &screenBounds)
      {
        scenario_tests::CaptureContentBounds result;
        if (!screenBounds.available || screenBounds.right <= screenBounds.left || screenBounds.bottom <= screenBounds.top)
        {
          return result;
        }
        result.available = true;
        result.right = screenBounds.right - screenBounds.left;
        result.bottom = screenBounds.bottom - screenBounds.top;
        return result;
      }

      /** Owns the crop-external native marker used by MAME's live screen
          completion seam. */
      class HostCompletionSignal
      {
      public:
        HostCompletionSignal()
            : window_(0)
        {
        }

        ~HostCompletionSignal()
        {
          if (this->window_)
          {
            DisposeWindow(this->window_);
            this->window_ = 0;
          }
        }

        bool publish()
        {
          if (this->window_)
          {
            return true;
          }
          Rect bounds = qd.screenBits.bounds;
          if (bounds.right - bounds.left < 16 || bounds.bottom - bounds.top < 16)
          {
            return false;
          }
          SetRect(&bounds, static_cast<short>(bounds.right - 12), static_cast<short>(bounds.bottom - 12),
                  static_cast<short>(bounds.right - 4), static_cast<short>(bounds.bottom - 4));
          Str255 title;
          title[0] = 0;
          this->window_ = NewWindow(0, &bounds, title, true, plainDBox, reinterpret_cast<WindowPtr>(-1L), false, 0);
          if (!this->window_)
          {
            return false;
          }
          GrafPtr previousPort = 0;
          GetPort(&previousPort);
          BeginUpdate(this->window_);
          SetPort(this->window_);
          FillRect(&this->window_->portRect, &qd.black);
          EndUpdate(this->window_);
          SetPort(previousPort);
          return true;
        }

      private:
        HostCompletionSignal(const HostCompletionSignal &);
        HostCompletionSignal &operator=(const HostCompletionSignal &);
        WindowPtr window_;
      };

      std::string CapturePath()
      {
        return dsl::SnapTestConfig::resolveCapturePath(kCaptureFile, kConfigPath);
      }

      dsl::SnapWriteStatus WriteRecord(const dsl::SnapTestConfig::Settings &settings, const dsl::SnapRecord &record)
      {
        const std::string path = CapturePath();
        const long maxBytes = settings.hasMaxTotalBytes ? settings.maxTotalBytes : 0;
        const long maxRecords = settings.hasMaxFiles ? settings.maxFiles : 0;
        return dsl::SnapFileWriter::appendRecordStatusWithLimits(path.c_str(), record, maxBytes, maxRecords);
      }

      dsl::SnapWriteStatus WriteCaptureMetadata(const dsl::SnapTestConfig::Settings &settings,
                                                const char *scenario,
                                                long tick,
                                                const scenario_tests::CaptureContentBounds &bounds)
      {
        dsl::SnapRecord record;
        record.setInt("format_version", 1);
        record.setInt("schema_version", 1);
        record.setInt("scenario_version", 1);
        record.set("test", "ScrapbookUI.capture.toolbox");
        record.set("step", scenario ? scenario : "startup");
        record.set("node", "ToolboxWindow");
        record.setInt("tick", tick);
        record.set("status", bounds.available ? dsl::SnapStatusOk() : dsl::SnapStatusError());
        if (bounds.available)
        {
          record.setInt("crop_left", bounds.left);
          record.setInt("crop_top", bounds.top);
          record.setInt("crop_right", bounds.right);
          record.setInt("crop_bottom", bounds.bottom);
        }
        const std::string path = dsl::SnapTestConfig::resolveCapturePath(kCaptureMetadataFile, kConfigPath);
        return dsl::SnapFileWriter::appendRecordStatusWithLimits(
            path.c_str(), record, settings.hasMaxTotalBytes ? settings.maxTotalBytes : 0,
            settings.hasMaxFiles ? settings.maxFiles : 0);
      }

      class ScenarioAppConfig : public AppConfigurable
      {
      public:
        ScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : AppConfigurable(context),
              settings_(settings),
              scenario_(settings.scenario),
              borrowedApp_(0),
              borrowedMainNode_(0),
              recorded_(false),
              tickCount_(0),
              lingerRemaining_(settings.hasLingerSeconds ? static_cast<double>(settings.lingerSeconds) : 0.0)
        {
        }

        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }

        virtual void compose(AppComposition &composition)
        {
          ObservedMainDefinition mainDefinition(scrapbook::MainProps().platformContext(this->getPlatformContext()),
                                                &this->borrowedMainNode_);
          composition << WindowDef(WindowProps()
                                       .frame(40, 40, 340, 250)
                                       .scene(mainDefinition)
                                       .title("LokaTestsToolbox")
                                       .visible(true)
                                       .idlePolicy(app::IdlePolicy::everyTick())
                                       .onIdle(&ScenarioAppConfig::OnWindowIdle, this));
        }

      private:
        static void OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
        {
          ScenarioAppConfig *self = static_cast<ScenarioAppConfig *>(userData);
          if (self)
          {
            self->tick(window, elapsedSeconds);
          }
        }

        void tick(Window *window, double elapsedSeconds)
        {
          ++this->tickCount_;
          if (!this->recorded_)
          {
            dsl::SnapRecord record;
            bool done = false;
            if (!this->borrowedMainNode_)
            {
              record = scenario_tests::MakeDriverErrorRecord(
                  this->settings_.scenario.c_str(), 2303, "MainNode was not mounted");
              done = true;
            }
            else
            {
              const scenario_tests::CaptureContentBounds captureBounds = QueryContentBounds(window);
              done = this->scenario_.step(
                  this->tickCount_, *this->borrowedMainNode_, ContentLocalBounds(captureBounds), record);
              if (done)
              {
                (void)WriteCaptureMetadata(
                    this->settings_, this->settings_.scenario.c_str(), this->tickCount_, captureBounds);
              }
            }
            if (done && this->borrowedMainNode_)
            {
              BoundaryAuditCounts counts;
              AuditProjectedTextBoundaries(this->borrowedMainNode_, 0, counts);
              if (counts.untagged != 0 || counts.tagged == 0)
              {
                record = scenario_tests::MakeDriverErrorRecord(
                    this->settings_.scenario.c_str(), 2304, "projected text context lost its boundary tag");
              }
            }
            if (done)
            {
              (void)WriteRecord(this->settings_, record);
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

        const dsl::SnapTestConfig::Settings settings_;
        scenario_tests::ScrapbookScenario scenario_;
        App *borrowedApp_;
        scrapbook::MainNode *borrowedMainNode_;
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
      if (!dsl::SnapTestConfig::load(kConfigPath, settings))
      {
        (void)WriteRecord(
            settings, scenario_tests::MakeDriverErrorRecord("startup", 2300, "LokaTest.cfg is missing or invalid"));
        return 0;
      }
      if (!settings.hasScenario)
      {
        (void)WriteRecord(settings, scenario_tests::MakeDriverErrorRecord("startup", 2301, "scenario is missing"));
        return 0;
      }
      if (!scenario_tests::IsRegisteredScenario(settings.scenario))
      {
        (void)WriteRecord(settings, scenario_tests::MakeDriverErrorRecord(
                                        settings.scenario.c_str(), 2302, "scenario is not registered"));
        return 0;
      }

      platform::InitPlatformRuntime();
      {
        core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
        assert(platformContext.get() && "PlatformContext is required");
        ScenarioAppConfig config(platformContext.get(), settings);
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
