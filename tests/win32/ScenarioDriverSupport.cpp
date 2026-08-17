#include "ScenarioDriverSupport.hpp"

#include <cstdio>
#include <sstream>
#include <string>

#include "ScenarioProfile.hpp"
#include "Win32Window.hpp"
#include "core/String.hpp"
#include "platform/Win32String.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/file/FileIO.hpp"
#include "testing/scene/ScenarioAudit.hpp"

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
    } // namespace

    class ScenarioRunState::Impl
    {
    public:
      Impl(const dsl::SnapTestConfig::Settings &settings, ScenarioRunMode mode)
          : settings_(settings),
            audit_(ArtifactFile(settings, kActualAudit), settings.scenario.c_str()),
            completed_(false),
            tick_(0),
            lingerRemaining_(settings.hasLingerSeconds ? settings.lingerSeconds : kDefaultLingerSeconds),
            exitCode_(audit_.isValid() ? 0 : 1)
      {
        switch (mode)
        {
        case SCENARIO_RUN_MODE_FLOW:
          break;
        }
      }

      dsl::testing::ScenarioAuditSink *audit()
      {
        return &this->audit_;
      }

      int exitCode() const
      {
        return this->exitCode_;
      }

      void tick(Window *window, App *app, scenario_tests::ScenarioDriver &driver, double elapsedSeconds)
      {
        if (this->completed_)
        {
          this->lingerRemaining_ -= elapsedSeconds;
          if (this->lingerRemaining_ <= 0.0 && app)
          {
            app->quit();
          }
          return;
        }

        ++this->tick_;
        const scenario_tests::CaptureContentBounds captureBounds = QueryCaptureContentBounds(window);
        dsl::SnapRecord record;
        const scenario_tests::ScenarioAdvance advance =
            driver.step(this->tick_, window, ContentLocalBounds(captureBounds), record);
        switch (advance)
        {
        case scenario_tests::SCENARIO_ADVANCE_PENDING:
          return;
        case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
          this->fail("rig scenario unexpectedly retained terminal ownership", app);
          return;
        case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
          break;
        }

        if (!driver.publishVerdict(record))
        {
          this->fail("could not publish the durable audit verdict", app);
          return;
        }
        if (!PublishCaptureFacts(this->settings_, window, captureBounds))
        {
          this->fail("could not publish capture bounds/profile", app);
          return;
        }
        if (!PublishTextArtifact(this->settings_, kCompletionMarker, "audit-ready\n"))
        {
          this->fail("could not publish the atomic completion marker", app);
          return;
        }
        this->completed_ = true;
      }

    private:
      void fail(const char *message, App *app)
      {
        std::fprintf(stderr, "win32 scenario: %s\n", message ? message : "failed");
        std::fflush(stderr);
        this->exitCode_ = 1;
        if (app)
        {
          app->quit();
        }
      }

      const dsl::SnapTestConfig::Settings settings_;
      dsl::testing::ScenarioAuditFile audit_;
      bool completed_;
      long tick_;
      double lingerRemaining_;
      int exitCode_;
    };

    ScenarioRunState::ScenarioRunState(const dsl::SnapTestConfig::Settings &settings, ScenarioRunMode mode)
        : impl_(new Impl(settings, mode))
    {
    }

    ScenarioRunState::~ScenarioRunState() {}

    dsl::testing::ScenarioAuditSink *ScenarioRunState::audit()
    {
      return this->impl_->audit();
    }

    int ScenarioRunState::exitCode() const
    {
      return this->impl_->exitCode();
    }

    void ScenarioRunState::tick(Window *window, App *app, scenario_tests::ScenarioDriver &driver, double elapsedSeconds)
    {
      this->impl_->tick(window, app, driver, elapsedSeconds);
    }

    bool LoadScenarioSettings(dsl::SnapTestConfig::Settings &settings, ScenarioRunMode &mode)
    {
      mode = SCENARIO_RUN_MODE_FLOW;
      return dsl::SnapTestConfig::load(kConfigPath, settings) && settings.hasScenario && settings.hasCaptureDir;
    }

    int WriteConfigurationErrorAudit(const dsl::SnapTestConfig::Settings &settings,
                                     const char *scenario,
                                     const dsl::SnapRecord &record)
    {
      dsl::testing::ScenarioAuditFile audit(ArtifactFile(settings, kActualAudit), scenario);
      dsl::testing::scenario_audit_detail::TerminalEmitter terminal(&audit);
      (void)(audit.isValid() && terminal.emit(dsl::testing::SCENARIO_AUDIT_FAILED, record));
      std::string message;
      (void)record.get("error_msg", message);
      std::fprintf(stderr, "win32 scenario: %s\n", message.empty() ? "configuration is invalid" : message.c_str());
      return 2;
    }
  } // namespace win32_scenario_tests
} // namespace loka
