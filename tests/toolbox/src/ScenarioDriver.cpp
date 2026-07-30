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
#include "core/util/ScopedPtr.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";
      const char *kCaptureFile = "LokaTestsToolbox.snap";

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

      ContentBounds QueryContentBounds(Window *window)
      {
        ContentBounds result;
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

      class ScenarioAppConfig : public AppConfigurable
      {
      public:
        ScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : AppConfigurable(context),
              settings_(settings),
              borrowedApp_(0),
              borrowedMainNode_(0)
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
          (void)elapsedSeconds;
          ScenarioAppConfig *self = static_cast<ScenarioAppConfig *>(userData);
          if (self)
          {
            self->finish(window);
          }
        }

        void finish(Window *window)
        {
          dsl::SnapRecord record;
          if (!this->borrowedMainNode_)
          {
            record = MakeDriverErrorRecord(this->settings_.scenario.c_str(), 2303, "MainNode was not mounted");
          }
          else
          {
            record =
                RunRegisteredScenario(this->settings_.scenario, *this->borrowedMainNode_, QueryContentBounds(window));
          }
          (void)WriteRecord(this->settings_, record);
          if (this->borrowedApp_)
          {
            this->borrowedApp_->quit();
          }
        }

        const dsl::SnapTestConfig::Settings settings_;
        App *borrowedApp_;
        scrapbook::MainNode *borrowedMainNode_;
      };
    } // namespace

    int RunScenarioApplication()
    {
      dsl::SnapTestConfig::Settings settings;
      if (!dsl::SnapTestConfig::load(kConfigPath, settings))
      {
        (void)WriteRecord(settings, MakeDriverErrorRecord("startup", 2300, "LokaTest.cfg is missing or invalid"));
        return 0;
      }
      if (!settings.hasScenario)
      {
        (void)WriteRecord(settings, MakeDriverErrorRecord("startup", 2301, "scenario is missing"));
        return 0;
      }
      if (!IsRegisteredScenario(settings.scenario))
      {
        (void)WriteRecord(settings,
                          MakeDriverErrorRecord(settings.scenario.c_str(), 2302, "scenario is not registered"));
        return 0;
      }

      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      ScenarioAppConfig config(platformContext.get(), settings);
      core::ScopedPtr<App> app(platformContext->createApp(&config, 0, 0));
      assert(app.get() && "App is required");
      config.setApp(app.get());
      app->run();
      return 0;
    }
  } // namespace toolbox_tests
} // namespace loka
