#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

#include "MainNode.hpp"
#include "ScenarioProfile.hpp"
#include "ScenarioWindow.hpp"
#include "ScrapbookScenarios.hpp"
#include "Win32Window.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/Window.hpp"
#include "core/String.hpp"
#include "core/util/ScopedPtr.hpp"
#include "platform/Win32String.hpp"
#include "platform/file/FileIO.hpp"
#include "platform/file/FileHandle.hpp"
#include "testing/scene/ScenarioAudit.hpp"

#if !defined(TEST_BUILD)
#error LokaScrapbookScenarioWin32 requires TEST_BUILD
#endif

namespace loka
{
  namespace win32_scenario_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";
      const char *kActualAudit = "actual.audit";
      const char *kActualProfile = "actual.profile";
      const char *kCaptureBounds = "capture.bounds";
      const char *kCompletionMarker = "LokaTestsWin32.complete";
      const long kDefaultLingerSeconds = 120;

      std::string JoinPath(const std::string &base, const char *leaf)
      {
        if (base.empty() || base == ".")
        {
          return leaf ? leaf : "";
        }
        const char last = base[base.size() - 1];
        return last == '/' || last == '\\' ? base + (leaf ? leaf : "") : base + "/" + (leaf ? leaf : "");
      }

      std::string ArtifactPath(const dsl::SnapTestConfig::Settings &settings, const char *leaf)
      {
        return JoinPath(settings.hasCaptureDir ? settings.captureDir : std::string("."), leaf);
      }

      platform::file::FileHandle ArtifactFile(const dsl::SnapTestConfig::Settings &settings, const char *leaf)
      {
        const std::string path = ArtifactPath(settings, leaf);
        platform::file::FileHandle result;
        result.displayPath = core::String::Utf8(path.data(), path.size());
        return result;
      }

      bool MoveFileAtomically(const std::string &temporary, const std::string &destination)
      {
        std::wstring temporaryWide;
        std::wstring destinationWide;
        const core::String temporaryString = core::String::Utf8(temporary.data(), temporary.size());
        const core::String destinationString = core::String::Utf8(destination.data(), destination.size());
        return win32::MaterializeWideString(temporaryString, temporaryWide)
               && win32::MaterializeWideString(destinationString, destinationWide)
               && MoveFileExW(temporaryWide.c_str(),
                              destinationWide.c_str(),
                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
                      != 0;
      }

      bool
      PublishTextArtifact(const dsl::SnapTestConfig::Settings &settings, const char *leaf, const std::string &content)
      {
        const std::string destination = ArtifactPath(settings, leaf);
        const std::string temporary = destination + ".tmp";
        platform::file::FileHandle file;
        file.displayPath = core::String::Utf8(temporary.data(), temporary.size());
        std::FILE *output = platform::file::OpenWriteTruncate(file);
        if (!output)
        {
          return false;
        }
        const bool wrote = std::fwrite(content.data(), 1, content.size(), output) == content.size();
        const bool flushed = wrote && platform::file::FlushWrite(output, file);
        const bool closed = std::fclose(output) == 0;
        return flushed && closed && MoveFileAtomically(temporary, destination);
      }

      scenario_tests::CaptureContentBounds QueryCaptureContentBounds(Window *window)
      {
        scenario_tests::CaptureContentBounds result;
        Win32Window *nativeWindow = window ? window->asWin32Window() : 0;
        const HWND hwnd = nativeWindow ? nativeWindow->hwnd() : 0;
        RECT windowRect;
        RECT clientRect;
        POINT topLeft = {0, 0};
        POINT bottomRight = {0, 0};
        if (!hwnd || !GetWindowRect(hwnd, &windowRect) || !GetClientRect(hwnd, &clientRect))
        {
          return result;
        }
        bottomRight.x = clientRect.right;
        bottomRight.y = clientRect.bottom;
        if (!ClientToScreen(hwnd, &topLeft) || !ClientToScreen(hwnd, &bottomRight))
        {
          return result;
        }
        result.available = true;
        result.left = topLeft.x - windowRect.left;
        result.top = topLeft.y - windowRect.top;
        result.right = bottomRight.x - windowRect.left;
        result.bottom = bottomRight.y - windowRect.top;
        return result;
      }

      scenario_tests::CaptureContentBounds ContentLocalBounds(const scenario_tests::CaptureContentBounds &captureBounds)
      {
        scenario_tests::CaptureContentBounds result;
        if (!captureBounds.available || captureBounds.right <= captureBounds.left
            || captureBounds.bottom <= captureBounds.top)
        {
          return result;
        }
        result.available = true;
        result.right = captureBounds.right - captureBounds.left;
        result.bottom = captureBounds.bottom - captureBounds.top;
        return result;
      }

      std::string ArchitectureName()
      {
#if defined(_M_ARM64)
        return "arm64";
#elif defined(_M_X64)
        return "x64";
#elif defined(_M_IX86)
        return "x86";
#else
        return "unknown";
#endif
      }

      std::string OsBuild()
      {
        typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW *);
        HMODULE module = GetModuleHandleW(L"ntdll.dll");
        RtlGetVersionFn rtlGetVersion =
            module ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(module, "RtlGetVersion")) : 0;
        OSVERSIONINFOW version;
        ZeroMemory(&version, sizeof(version));
        version.dwOSVersionInfoSize = sizeof(version);
        if (!rtlGetVersion || rtlGetVersion(&version) != 0)
        {
          return "unknown";
        }
        std::ostringstream output;
        output << version.dwMajorVersion << '.' << version.dwMinorVersion << '.' << version.dwBuildNumber;
        return output.str();
      }

      bool PublishCaptureFacts(const dsl::SnapTestConfig::Settings &settings,
                               Window *window,
                               const scenario_tests::CaptureContentBounds &bounds)
      {
        Win32Window *nativeWindow = window ? window->asWin32Window() : 0;
        RECT windowRect;
        if (!nativeWindow || !nativeWindow->hwnd() || !bounds.available
            || !GetWindowRect(nativeWindow->hwnd(), &windowRect))
        {
          return false;
        }

        std::ostringstream boundsText;
        boundsText << "bounds_version=1\n"
                   << "left=" << bounds.left << "\n"
                   << "top=" << bounds.top << "\n"
                   << "right=" << bounds.right << "\n"
                   << "bottom=" << bounds.bottom << "\n";

        int scalePercent = 0;
        int depth = 0;
        Window::DisplayAppearance appearance = Window::DISPLAY_APPEARANCE_LIGHT;
        const bool hasScale = window->queryDisplayScalePercent(scalePercent);
        const bool hasDepth = window->queryDisplayDepth(depth);
        const bool hasAppearance = window->queryDisplayAppearance(appearance);
        typedef scenario_tests::ProfileFact<int> IntFact;
        typedef scenario_tests::ProfileFact<std::string> StringFact;
        const scenario_tests::ScenarioProfile profile(
            OsBuild(),
            ArchitectureName(),
            hasScale ? IntFact::available(scalePercent) : IntFact::unavailable(),
            hasDepth ? IntFact::available(depth) : IntFact::unavailable(),
            hasAppearance ? StringFact::available(appearance == Window::DISPLAY_APPEARANCE_DARK ? "dark" : "light")
                          : StringFact::unavailable(),
            "PrintWindow.PW_RENDERFULLCONTENT.v1",
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top);
        return PublishTextArtifact(settings, kCaptureBounds, boundsText.str())
               && PublishTextArtifact(settings, kActualProfile, profile.render());
      }

      bool WriteDriverErrorAudit(const dsl::SnapTestConfig::Settings &settings,
                                 const char *scenario,
                                 long errorCode,
                                 const char *message)
      {
        dsl::testing::ScenarioAuditFile audit(ArtifactFile(settings, kActualAudit), scenario);
        dsl::testing::scenario_audit_detail::TerminalEmitter terminal(&audit);
        return audit.isValid()
               && terminal.emit(dsl::testing::SCENARIO_AUDIT_FAILED,
                                scenario_tests::MakeDriverErrorRecord(scenario, errorCode, message));
      }

      class ScenarioAppConfig : public AppConfigurable
      {
      public:
        ScenarioAppConfig(PlatformContext *context,
                          const dsl::SnapTestConfig::Settings &settings,
                          const scenario_tests::ScenarioLaunchPlan &launchPlan)
            : AppConfigurable(context),
              settings_(settings),
              audit_(ArtifactFile(settings, kActualAudit), launchPlan.scenario().c_str()),
              scenario_(launchPlan, &this->audit_),
              borrowedApp_(0),
              borrowedMainNode_(0),
              completed_(false),
              tick_(0),
              lingerRemaining_(settings.hasLingerSeconds ? settings.lingerSeconds : kDefaultLingerSeconds),
              exitCode_(audit_.isValid() ? 0 : 1)
        {
        }

        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }

        int exitCode() const
        {
          return this->exitCode_;
        }

        virtual void compose(AppComposition &composition)
        {
          composition << scenario_tests::MakeScenarioWindow<scrapbook::MainProps, scrapbook::MainNode>(
              scrapbook::MainProps().platformContext(this->getPlatformContext()),
              &this->borrowedMainNode_,
              340,
              250,
              "LokaScrapbookScenarioWin32",
              app::IdlePolicy::everyTick(),
              &ScenarioAppConfig::OnWindowIdle,
              this);
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

        void fail(const char *message)
        {
          std::fprintf(stderr, "win32 scenario: %s\n", message ? message : "failed");
          std::fflush(stderr);
          this->exitCode_ = 1;
          if (this->borrowedApp_)
          {
            this->borrowedApp_->quit();
          }
        }

        void tick(Window *window, double elapsedSeconds)
        {
          if (this->completed_)
          {
            this->lingerRemaining_ -= elapsedSeconds;
            if (this->lingerRemaining_ <= 0.0 && this->borrowedApp_)
            {
              this->borrowedApp_->quit();
            }
            return;
          }
          if (!window || !this->borrowedMainNode_)
          {
            return;
          }

          ++this->tick_;
          const scenario_tests::CaptureContentBounds captureBounds = QueryCaptureContentBounds(window);
          dsl::SnapRecord record;
          const scenario_tests::ScenarioAdvance advance = this->scenario_.step(
              this->tick_, window->scene(), *this->borrowedMainNode_, ContentLocalBounds(captureBounds), record);
          switch (advance)
          {
          case scenario_tests::SCENARIO_ADVANCE_PENDING:
            return;
          case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
            this->fail("rig scenario unexpectedly retained terminal ownership");
            return;
          case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
            break;
          }

          if (!this->scenario_.publishVerdict(record))
          {
            this->fail("could not publish the durable audit verdict");
            return;
          }
          if (!PublishCaptureFacts(this->settings_, window, captureBounds))
          {
            this->fail("could not publish capture bounds/profile");
            return;
          }
          if (!PublishTextArtifact(this->settings_, kCompletionMarker, "audit-ready\n"))
          {
            this->fail("could not publish the atomic completion marker");
            return;
          }
          this->completed_ = true;
        }

        const dsl::SnapTestConfig::Settings settings_;
        dsl::testing::ScenarioAuditFile audit_;
        scenario_tests::ScrapbookScenario scenario_;
        App *borrowedApp_;
        scrapbook::MainNode *borrowedMainNode_;
        bool completed_;
        long tick_;
        double lingerRemaining_;
        int exitCode_;
      };
    } // namespace

    int RunScenarioApplication(HINSTANCE hInstance, int nCmdShow)
    {
      dsl::SnapTestConfig::Settings settings;
      const bool configLoaded = dsl::SnapTestConfig::load(kConfigPath, settings);
      scenario_tests::ScenarioLaunchPlan launchPlan;
      if (!scenario_tests::QueryRigLaunchPlan(configLoaded, settings, launchPlan) || !settings.hasCaptureDir)
      {
        const char *scenario = settings.hasScenario ? settings.scenario.c_str() : "startup";
        (void)WriteDriverErrorAudit(settings, scenario, 2310, "LokaTest.cfg is missing or invalid");
        std::fprintf(stderr, "win32 scenario: LokaTest.cfg is missing or invalid\n");
        return 2;
      }

      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      ScenarioAppConfig config(platformContext.get(), settings, launchPlan);
      if (config.exitCode() != 0)
      {
        return config.exitCode();
      }
      core::ScopedPtr<App> app(platformContext->createApp(&config, hInstance, nCmdShow));
      assert(app.get() && "App is required");
      if (!app.get())
      {
        return 1;
      }
      config.setApp(app.get());
      app->run();
      return config.exitCode();
    }
  } // namespace win32_scenario_tests
} // namespace loka

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::win32_scenario_tests::RunScenarioApplication(hInstance, nCmdShow);
}
