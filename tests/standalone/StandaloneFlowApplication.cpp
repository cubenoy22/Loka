#include "StandaloneFlowApplication.hpp"

#include <cassert>

#include "MainNode.hpp"
#include "ScrapbookScenarios.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "core/io/File.hpp"
#include "core/util/ScopedPtr.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileHandle.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace standalone_tests
  {
    namespace
    {
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
            *this->observed_ = node ? static_cast<scrapbook::MainNode *>(node) : 0;
          }
          return node;
        }

      private:
        scrapbook::MainNode **observed_;
      };

      scenario_tests::CaptureContentBounds ContentBounds(Window *window)
      {
        scenario_tests::CaptureContentBounds result;
        if (!window)
        {
          return result;
        }
        const core::Frame frame = window->frameState().get();
        if (frame.width <= 0 || frame.height <= 0)
        {
          return result;
        }
        result.available = true;
        result.right = frame.width;
        result.bottom = frame.height;
        return result;
      }

      platform::file::FileHandle ResolveAuditFile()
      {
        platform::file::FileHandle result;
        if (!platform::file::ResolveApplicationSidecar(file::File::Application() << file::File("LOG.TXT"), result))
        {
          return platform::file::FileHandle();
        }
        return result;
      }

      class StandaloneFlowAppConfig : public AppConfigurable
      {
      public:
        explicit StandaloneFlowAppConfig(PlatformContext *context)
            : AppConfigurable(context),
              audit_(ResolveAuditFile(), "standalone-tour"),
              scenario_(scenario_tests::ScenarioLaunchPlan::StandaloneTour(), &this->audit_),
              borrowedMainNode_(0),
              tick_(0)
        {
        }

        virtual ~StandaloneFlowAppConfig()
        {
          this->scenario_.stop();
        }

        bool isValid() const
        {
          return this->audit_.isValid();
        }

        virtual void compose(AppComposition &composition)
        {
          ObservedMainDefinition mainDefinition(scrapbook::MainProps().platformContext(this->getPlatformContext()),
                                                &this->borrowedMainNode_);
          composition << WindowDef(WindowProps()
                                       .frame(40, 40, 340, 250)
                                       .scene(mainDefinition)
                                       .title("Loka Scrapbook Standalone Flow")
                                       .visible(true)
                                       .idlePolicy(app::IdlePolicy::interval(0.1))
                                       .onIdle(&StandaloneFlowAppConfig::OnWindowIdle, this));
        }

      private:
        static void OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
        {
          (void)elapsedSeconds;
          StandaloneFlowAppConfig *self = static_cast<StandaloneFlowAppConfig *>(userData);
          if (self)
          {
            self->tick(window);
          }
        }

        void tick(Window *window)
        {
          ++this->tick_;
          if (!window || !this->borrowedMainNode_)
          {
            return;
          }
          dsl::SnapRecord record;
          const scenario_tests::ScenarioAdvance advance = this->scenario_.step(
              this->tick_, window->scene(), *this->borrowedMainNode_, ContentBounds(window), record);
          switch (advance)
          {
          case scenario_tests::SCENARIO_ADVANCE_PENDING:
          case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
          case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
            return;
          }
        }

        dsl::testing::ScenarioAuditFile audit_;
        scenario_tests::ScrapbookScenario scenario_;
        scrapbook::MainNode *borrowedMainNode_;
        long tick_;
      };
    } // namespace

    int RunStandaloneFlowApplication()
    {
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      StandaloneFlowAppConfig config(platformContext.get());
      if (!config.isValid())
      {
        return 1;
      }
      core::ScopedPtr<App> app(platformContext->createApp(&config, 0, 0));
      assert(app.get() && "App is required");
      if (!app.get())
      {
        return 1;
      }
      app->run();
      return 0;
    }
  } // namespace standalone_tests
} // namespace loka
