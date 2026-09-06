#include "SimpleViewerScenarioDriver.hpp"

#include <cassert>
#include <MacMemory.h>

#include "MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"
#include "ScenarioDriverSupport.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/scene/ScenarioAudit.hpp"
#include "testing/scene/SceneTestFlow.hpp"

/** Test-only access to the production menu, chooser result and completed image.
    No dialog is mounted; the same ImageLoadSession receives the same result. */
class SimpleViewerTestAccess
{
public:
  static simpleviewer::MainProps props(SimpleViewerAppConfig &config)
  {
    return simpleviewer::MainProps()
        .platformContext(config.getPlatformContext())
        .openDialogEvent(&config.openDialogEvent_)
        .displayMode(config.menu_.displayModeState())
        .fitEvent(config.menu_.fitEvent())
        .actualEvent(config.menu_.actualEvent())
        .actualScrollEvent(config.menu_.actualScrollEvent());
  }

  static void open(simpleviewer::MainNode &node, const loka::app::FileChooserResult &result)
  {
    loka::core::StateTrackerGuard guard(node.tracker());
    node.imageLoad_.begin(node, node.props.platformContext_, node.chooserResult_.state(),
                         static_cast<loka::core::PushStateTracker *>(node.tracker()));
    node.chooserResult_.set(result, true);
  }

  static bool capture(const simpleviewer::MainNode &node, loka::dsl::SnapRecord &record)
  {
    const loka::core::resource::Image &image = node.image_.get();
    record.setInt("image.width", image.width());
    record.setInt("image.height", image.height());
    if (image.isValid())
    {
      record.set("image.load", "ok");
      return true;
    }
    // The production session consumes handled errors and releases its Flow.
    // Decode its completed message using the production formatter, not a
    // second error-message table or a callback that changes Flow lifetime.
    using namespace simpleviewer;
    const SimpleViewerFlowErrorCode codes[] = {
      SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_CONTEXT_MISSING,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_DECODE_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_OPENFILE_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_NO_FSSPEC,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_OPEN_DF_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_GETEOF_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_READ_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_SEEK_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_READ_FAILED,
      SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_LOAD_REQUIRES_RELEASE
    };
    for (unsigned i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
    {
      loka::dsl::FlowError error;
      error.code = codes[i];
      if (node.chooserMessage_.get().equals(ImageLoadSession::buildErrorMessage(error)))
      {
        record.setInt("image.load", codes[i]);
        return true;
      }
    }
    return false;
  }
};

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      dsl::SnapRecord MakeRecord(const char *scenario, long tick, const char *status)
      {
        dsl::SnapRecord record;
        dsl::FlowError error;
        dsl::BuildSnapV1RecordAdapter("SimpleViewer", scenario, "SimpleViewer.Image", tick, 1, status)
            .run(0, record, error);
        return record;
      }

      /** Measurement owner; the borrowed MainNode is used only during idle
          while its App-owned Window/Scene are alive, never during teardown. */
      class SimpleViewerScenarioAppConfig : public SimpleViewerAppConfig
      {
      public:
        SimpleViewerScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : SimpleViewerAppConfig(context), scenario_(settings.scenario),
              audit_(ResolveScenarioAuditFile(), settings.scenario.c_str()), terminal_(&this->audit_),
              borrowedApp_(0), borrowedMain_(0), tick_(0),
              lingerRemaining_(settings.hasLingerSeconds ? settings.lingerSeconds : 0.0),
              loadFacts_(), completionPublisher_()
        {
        }

        void setApp(App *app) { this->borrowedApp_ = app; }

        virtual void compose(AppComposition &composition)
        {
          // Presentation twin of SimpleViewerAppConfig::compose. Move the
          // production 480x280 content below the rig's menu/title bars.
          scenario_tests::ObservedMainDefinition<simpleviewer::MainProps, simpleviewer::MainNode> mainDefinition(
              SimpleViewerTestAccess::props(*this), &this->borrowedMain_);
          composition << WindowDef(WindowProps().frame(16, 41, 480, 280)
                                       .scene(mainDefinition).title("LokaSimpleViewer").visible(true)
                                       .idlePolicy(app::IdlePolicy::everyTick())
                                       .onIdle(&SimpleViewerScenarioAppConfig::OnIdle, this));
        }

      private:
        static void OnIdle(Window *window, double elapsedSeconds, void *userData)
        {
          static_cast<SimpleViewerScenarioAppConfig *>(userData)->tick(window, elapsedSeconds);
        }

        bool recordStep(const char *name)
        {
          return this->audit_.recordStep(dsl::testing::ScenarioStepTerminal(
              static_cast<int>(this->tick_), name, this->tick_, this->tick_,
              dsl::FLOW_STEP_SUCCEEDED, dsl::FlowError()));
        }

        bool openImage()
        {
          const file::File item = file::File::Application()
              << file::File(this->scenario_ == "open-12k" ? "SV12K.PICT" : "SV50K.PICT");
          platform::file::FileHandle handle;
          if (!this->getPlatformContext()->openFile(item, handle) || !handle.hasSpec) return false;
          short refNum = 0;
          if (FSpOpenDF(&handle.spec, fsRdPerm, &refNum) != noErr) return false;
          long bytes = 0;
          const OSErr sizeError = GetEOF(refNum, &bytes);
          const OSErr closeError = FSClose(refNum);
          if (sizeError != noErr || closeError != noErr || bytes < 0) return false;
          const app::FileChooserResult result = app::FileChooserResult::File(item);
          // Sample before the flow allocates. Record construction happens
          // afterwards so it cannot perturb the measured pre-load heap.
          const long freeBytes = FreeMem();
          std::size_t maxBlock = 0;
          if (!this->getPlatformContext()->queryLargestContiguousAllocation(maxBlock)) return false;
          SimpleViewerTestAccess::open(*this->borrowedMain_, result);
          this->loadFacts_.setInt("heap.free", freeBytes);
          this->loadFacts_.setInt("heap.max_block", static_cast<long>(maxBlock));
          this->loadFacts_.setInt("image.bytes", bytes);
          return this->recordStep("open-image");
        }

        void fail(Window *window)
        {
          const dsl::SnapRecord record = MakeRecord(this->scenario_.c_str(), this->tick_, dsl::SnapStatusError());
          (void)this->terminal_.emit(dsl::testing::SCENARIO_AUDIT_FAILED, record);
          (void)this->completionPublisher_.publish(window);
        }

        void tick(Window *window, double elapsedSeconds)
        {
          if (this->terminal_.isSettled())
          {
            this->lingerRemaining_ -= elapsedSeconds;
            if (this->lingerRemaining_ <= 0.0 && this->borrowedApp_) this->borrowedApp_->quit();
            return;
          }
          if (!window || !window->scene() || !this->borrowedMain_)
          {
            this->fail(window);
            return;
          }
          app::scene::Scene *scene = window->scene();
          ToolboxScenePlatformController *controller = static_cast<ToolboxScenePlatformController *>(
              dsl::testing::SceneTestAccess::platformController(*scene));
          if (!controller) { this->fail(window); return; }
          // Same settled-presentation wall as SmirkBench: retirement alone
          // does not account for pending Toolbox paint.
          if (scene->hasPendingInvalidation() || controller->hasPendingSync()
              || window->asToolboxWindow()->hasPendingInvalidate()) return;
          ++this->tick_;
          if (this->tick_ < 2) return;
          if (this->tick_ == 2)
          {
            if (!this->recordStep("post-settle") || !this->openImage()) this->fail(window);
            return;
          }
          dsl::SnapRecord record = MakeRecord(this->scenario_.c_str(), this->tick_, dsl::SnapStatusOk());
          const char *keys[] = {"heap.free", "heap.max_block", "image.bytes"};
          for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
          {
            std::string value;
            if (!this->loadFacts_.get(keys[i], value)) { this->fail(window); return; }
            record.set(keys[i], value.c_str());
          }
          record.set("rail", "toolbox");
          record.set("checkpoint", "post-open");
          scenario_tests::SetContentBounds(record, ContentLocalBounds(QueryCaptureContentBounds(window)));
          if (!SimpleViewerTestAccess::capture(*this->borrowedMain_, record)
              || !this->recordStep("post-open") || !this->audit_.recordVerdict(record))
          {
            this->fail(window);
            return;
          }
          (void)this->terminal_.emit(dsl::testing::SCENARIO_AUDIT_SUCCEEDED);
          (void)this->completionPublisher_.publish(window);
        }

        const std::string scenario_;
        dsl::testing::ScenarioAuditFile audit_;
        dsl::testing::scenario_audit_detail::TerminalEmitter terminal_;
        App *borrowedApp_;
        simpleviewer::MainNode *borrowedMain_;
        long tick_;
        double lingerRemaining_;
        dsl::SnapRecord loadFacts_;
        ScenarioCompletionPublisher completionPublisher_;
      };
    } // namespace

    int RunSimpleViewerScenarioApplication()
    {
      dsl::SnapTestConfig::Settings settings;
      if (!dsl::SnapTestConfig::load("LokaTest.cfg", settings) || !settings.hasScenario
          || (settings.scenario != "open-12k" && settings.scenario != "open-50k"))
      {
        (void)WriteScenarioErrorAudit("startup", MakeRecord("startup", 0, dsl::SnapStatusError()));
        return 0;
      }
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> context(platform::CreatePlatformContext());
      assert(context.get() && "PlatformContext is required");
      if (!context.get()) return 1;
      SimpleViewerScenarioAppConfig config(context.get(), settings);
      core::ScopedPtr<App> app(context->createApp(&config, 0, 0));
      assert(app.get() && "App is required");
      if (!app.get()) return 1;
      config.setApp(app.get());
      app->run();
      return 0;
    }
  } // namespace toolbox_tests
} // namespace loka
