#include "HelloWorldScenarioDriver.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "HelloWorldScenarios.hpp"
#include "HelloWorldScenarioPresentation.hpp"
#include "ScenarioDriverSupport.hpp"
#include "StartupScenarios.hpp"
#include "ToolboxWindow.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";
      const char *kDefaultScenarioName = "toggle-action-probe";
      const char *kBmiRoundtripScenarioName = "bmi-roundtrip";
      const char *kDirtyReplayProbeKey = "probe_dirty_replay";

      /** The probe is opt-in until #404 widens the window: the BMI fields sit
          below the 420x300 fold, so on the tracked cell the chrome row is out
          of port and the probe would red the rail for visibility, not replay.
          run-scenario.sh --probe writes this key; the tracked cell does not.
          SnapTestConfig ignores unknown keys, so this stays driver-local. */
      bool DirtyReplayProbeRequested(const char *configPath)
      {
        FILE *fp = std::fopen(configPath, "rb");
        if (!fp)
        {
          return false;
        }
        char line[256];
        bool requested = false;
        while (std::fgets(line, sizeof(line), fp) != 0)
        {
          const char *cursor = line;
          while (*cursor == ' ' || *cursor == '\t')
          {
            ++cursor;
          }
          const size_t keyLength = std::strlen(kDirtyReplayProbeKey);
          if (std::strncmp(cursor, kDirtyReplayProbeKey, keyLength) != 0)
          {
            continue;
          }
          cursor += keyLength;
          while (*cursor == ' ' || *cursor == '\t')
          {
            ++cursor;
          }
          if (*cursor == '1')
          {
            requested = true;
          }
        }
        std::fclose(fp);
        return requested;
      }
      const char *kBmiHeightInputTestId = "HelloWorld.Bmi.HeightInput";
      const short kDirtyReplayProbeMaxWidth = 256;

      void CapturePixelRow(const Rect &row, unsigned char *pixels)
      {
        for (short x = row.left; x < row.right; ++x)
        {
          pixels[x - row.left] = GetPixel(x, row.top) ? 1 : 0;
        }
      }

      bool PixelRowMatches(const Rect &row, const unsigned char *pixels)
      {
        for (short x = row.left; x < row.right; ++x)
        {
          const unsigned char pixel = GetPixel(x, row.top) ? 1 : 0;
          if (pixel != pixels[x - row.left])
          {
            return false;
          }
        }
        return true;
      }

      bool ProbeBmiHeightDirtyReplay(Window *window, const char *&failureMessage)
      {
        failureMessage = "dirty replay probe was not run";
        ToolboxWindow *toolboxWindow = window ? window->asToolboxWindow() : 0;
        WindowPtr nativeWindow = toolboxWindow ? toolboxWindow->window() : 0;
        if (!window || !window->scene() || !toolboxWindow || !nativeWindow)
        {
          failureMessage = "dirty replay probe window was unavailable";
          return false;
        }

        app::EditTextNode *heightInput = 0;
        dsl::FlowError lookupError;
        if (dsl::testing::LookupNodeById<app::EditTextNode>(
                window->scene(), kBmiHeightInputTestId, heightInput, lookupError)
            != dsl::FLOW_STEP_SUCCEEDED)
        {
          failureMessage = "dirty replay probe could not find the BMI height input";
          return false;
        }
        ToolboxEditTextContext *editContext =
            heightInput ? static_cast<ToolboxEditTextContext *>(heightInput->getContext()) : 0;
        if (!editContext)
        {
          failureMessage = "dirty replay probe found no Toolbox EditText context";
          return false;
        }

        const Rect chrome = editContext->chromeRect();
        const int width = static_cast<int>(chrome.right) - static_cast<int>(chrome.left);
        if (width <= 0 || width > kDirtyReplayProbeMaxWidth || chrome.top < nativeWindow->portRect.top
            || chrome.top >= nativeWindow->portRect.bottom || chrome.left < nativeWindow->portRect.left
            || chrome.right > nativeWindow->portRect.right)
        {
          failureMessage = "dirty replay probe found invalid EditText chrome bounds";
          return false;
        }

        // EditText layout insets textRect.top by two pixels. Damage only the
        // one-pixel chrome row so replay has to gate on the extent it frames.
        Rect row = chrome;
        row.bottom = static_cast<short>(row.top + 1);
        unsigned char savedPixels[kDirtyReplayProbeMaxWidth];
        GrafPtr previousPort = 0;
        GetPort(&previousPort);
        SetPort(nativeWindow);
        CapturePixelRow(row, savedPixels);
        EraseRect(&row);
        const bool damageObserved = !PixelRowMatches(row, savedPixels);
        SetPort(previousPort);
        if (!damageObserved)
        {
          failureMessage = "dirty replay probe positive control did not change pixels";
          return false;
        }

        toolboxWindow->drawDirty(row);

        SetPort(nativeWindow);
        const bool replayRestoredPixels = PixelRowMatches(row, savedPixels);
        SetPort(previousPort);
        if (!replayRestoredPixels)
        {
          failureMessage = "dirty replay did not restore the EditText chrome row";
          return false;
        }
        failureMessage = 0;
        return true;
      }

      bool IsSuccessfulRecord(const dsl::SnapRecord &record)
      {
        std::string status;
        return record.get("status", status) && status == dsl::SnapStatusOk();
      }

      void SetDirtyReplayProbeFailure(dsl::SnapRecord &record, const char *failureMessage)
      {
        record = scenario_tests::MakeHelloWorldDriverErrorRecord(2404, failureMessage);
        record.set("step", kBmiRoundtripScenarioName);
        record.set("node", kBmiHeightInputTestId);
      }

      class HelloWorldScenarioAppConfig : public scenario_tests::HelloWorldScenarioPresentation
      {
      public:
        HelloWorldScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : scenario_tests::HelloWorldScenarioPresentation(context, HelloWorldMenuSeed::FromWallClock(0x13579BDFUL)),
              startup_(scenario_tests::IsStartupScenario(settings.scenario)),
              audit_(ResolveScenarioAuditFile(), settings.scenario.c_str()),
              startupScenario_(scenario_tests::STARTUP_EXAMPLE_HELLO_WORLD,
                               scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED,
                               &this->audit_),
              scenario_(settings.scenario,
                        scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED,
                        &this->audit_),
              borrowedApp_(0),
              recorded_(false),
              tickCount_(0),
              lingerRemaining_(settings.hasLingerSeconds ? static_cast<double>(settings.lingerSeconds) : 0.0),
              hostCompletionSignal_()
        {
        }

        virtual ~HelloWorldScenarioAppConfig()
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
            if (!window || !window->scene())
            {
              record = this->startup_ ? scenario_tests::MakeStartupDriverErrorRecord(
                                            scenario_tests::STARTUP_EXAMPLE_HELLO_WORLD, 2802, "Scene was not mounted")
                                      : scenario_tests::MakeHelloWorldDriverErrorRecord(2402, "Scene was not mounted");
              done = true;
            }
            else
            {
              const scenario_tests::CaptureContentBounds captureBounds = QueryCaptureContentBounds(window);
              const scenario_tests::ScenarioAdvance advance =
                  this->startup_ ? this->startupScenario_.step(
                                       this->tickCount_, window->scene(), ContentLocalBounds(captureBounds), record)
                                 : this->scenario_.step(
                                       this->tickCount_, window->scene(), ContentLocalBounds(captureBounds), record);
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
            if (done)
            {
              if (!this->startup_ && this->scenario_.name() == kBmiRoundtripScenarioName
                  && IsSuccessfulRecord(record) && DirtyReplayProbeRequested(kConfigPath))
              {
                // Success leaves the completed portable record untouched;
                // only a failed rail-local probe replaces the verdict.
                const char *probeFailure = 0;
                if (!ProbeBmiHeightDirtyReplay(window, probeFailure))
                {
                  SetDirtyReplayProbeFailure(record, probeFailure);
                }
              }
              if (this->startup_)
              {
                (void)this->startupScenario_.publishVerdict(record);
              }
              else
              {
                (void)this->scenario_.publishVerdict(record);
              }
              this->recorded_ = true;
              (void)this->hostCompletionSignal_.publish();
            }
          }
          if (!this->recorded_)
          {
            return;
          }
          this->lingerRemaining_ -= elapsedSeconds;
          if (this->lingerRemaining_ <= 0.0 && this->borrowedApp_)
          {
            this->borrowedApp_->quit();
          }
        }

        const bool startup_;
        dsl::testing::ScenarioAuditFile audit_;
        scenario_tests::StartupScenario startupScenario_;
        scenario_tests::HelloWorldScenario scenario_;
        App *borrowedApp_;
        bool recorded_;
        long tickCount_;
        double lingerRemaining_;
        HostCompletionSignal hostCompletionSignal_;
      };
    } // namespace

    int RunHelloWorldScenarioApplication()
    {
      dsl::SnapTestConfig::Settings settings;
      const bool configLoaded = dsl::SnapTestConfig::load(kConfigPath, settings);
      if (!configLoaded)
      {
        (void)WriteScenarioErrorAudit(
            kDefaultScenarioName,
            scenario_tests::MakeHelloWorldDriverErrorRecord(2400, "LokaTest.cfg is missing or invalid"));
        return 0;
      }
      if (!settings.hasScenario
          || (!scenario_tests::IsStartupScenario(settings.scenario)
              && !scenario_tests::IsHelloWorldScenario(settings.scenario)))
      {
        (void)WriteScenarioErrorAudit(
            settings.hasScenario ? settings.scenario.c_str() : kDefaultScenarioName,
            scenario_tests::MakeHelloWorldDriverErrorRecord(2401, "scenario is missing or not registered"));
        return 0;
      }

      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      HelloWorldScenarioAppConfig config(platformContext.get(), settings);
      core::ScopedPtr<App> app(platformContext->createApp(&config, 0, 0));
      assert(app.get() && "App is required");
      if (!app.get())
      {
        return 1;
      }
      config.setApp(app.get());
      app->run();
      return 0;
    }
  } // namespace toolbox_tests
} // namespace loka
