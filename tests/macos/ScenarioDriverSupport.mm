#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "MacObjCCompat.hpp"
#include "MacWindow.hpp"
#include "ScenarioDriverSupport.hpp"
#include "ScenarioProfile.hpp"
#include "platform/file/FileHandle.hpp"
#include "testing/MacWindowTestAccess.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace macos_scenario_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";
      const char *kActualAudit = "actual.audit";
      const char *kActualImage = "actual.png";
      const char *kActualProfile = "actual.profile";
      const char *kReadyMarker = "ready";
      const char *kReleaseMarker = "release";
      const char *kCompletionMarker = "complete";
      const long kMaximumSettleFrames = 120;

      bool ResolveRunMode(ScenarioRunMode &out)
      {
        const char *value = std::getenv("LOKA_MACOS_SCENARIO_MODE");
        if (!value || !*value || std::strcmp(value, "flow") == 0)
        {
          out = SCENARIO_RUN_MODE_FLOW;
          return true;
        }
        if (std::strcmp(value, "inspect") == 0)
        {
          out = SCENARIO_RUN_MODE_INSPECT;
          return true;
        }
        return false;
      }

      std::string JoinPath(const std::string &base, const char *leaf)
      {
        if (base.empty())
        {
          return leaf ? leaf : "";
        }
        if (base[base.size() - 1] == '/')
        {
          return base + (leaf ? leaf : "");
        }
        return base + "/" + (leaf ? leaf : "");
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

      bool CopyFile(const char *source, const char *destination)
      {
        FILE *input = std::fopen(source, "rb");
        if (!input)
        {
          return false;
        }
        FILE *output = std::fopen(destination, "wb");
        if (!output)
        {
          std::fclose(input);
          return false;
        }
        char buffer[4096];
        size_t count = 0;
        bool ok = true;
        while ((count = std::fread(buffer, 1, sizeof(buffer), input)) > 0)
        {
          if (std::fwrite(buffer, 1, count, output) != count)
          {
            ok = false;
            break;
          }
        }
        if (std::ferror(input) != 0)
        {
          ok = false;
        }
        if (std::fclose(output) != 0)
        {
          ok = false;
        }
        std::fclose(input);
        return ok;
      }

      bool WriteFile(const char *path, const std::string &content)
      {
        if (!path || !*path)
        {
          return false;
        }
        FILE *output = std::fopen(path, "wb");
        if (!output)
        {
          return false;
        }
        const bool wrote = std::fwrite(content.data(), 1, content.size(), output) == content.size();
        const bool closed = std::fclose(output) == 0;
        return wrote && closed;
      }

      bool ReadSysctlString(const char *name, std::string &out)
      {
        out.clear();
        size_t size = 0;
        if (!name || sysctlbyname(name, 0, &size, 0, 0) != 0 || size == 0)
        {
          return false;
        }
        char *buffer = new (std::nothrow) char[size];
        if (!buffer)
        {
          return false;
        }
        const bool ok = sysctlbyname(name, buffer, &size, 0, 0) == 0 && size > 0;
        if (ok)
        {
          out.assign(buffer, buffer[size - 1] == '\0' ? size - 1 : size);
        }
        delete[] buffer;
        return ok;
      }

      bool CaptureContentFrame(
          MacWindow &window, const char *path, unsigned long long &outHash, long &outPixelWidth, long &outPixelHeight)
      {
        NSView *view = (NSView *)dsl::testing::MacWindowTestAccess::contentView(window);
        if (!view || !path || !*path)
        {
          std::fprintf(stderr, "macos scenario capture: missing view or output path\n");
          return false;
        }

        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        [view displayIfNeeded];
        const NSRect bounds = [view bounds];
        NSBitmapImageRep *bitmap = [view bitmapImageRepForCachingDisplayInRect:bounds];
        if (!bitmap)
        {
          std::fprintf(stderr,
                       "macos scenario capture: "
                       "bitmapImageRepForCachingDisplayInRect returned nil"
                       " (bounds %.0fx%.0f, window=%s)\n",
                       bounds.size.width,
                       bounds.size.height,
                       [view window] ? "present" : "missing");
          [pool drain];
          return false;
        }
        [view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
        unsigned char *pixels = [bitmap bitmapData];
        const NSInteger bytesPerRow = [bitmap bytesPerRow];
        const NSInteger bitsPerPixel = [bitmap bitsPerPixel];
        const NSInteger pixelWidth = [bitmap pixelsWide];
        const NSInteger pixelHeight = [bitmap pixelsHigh];
        const NSInteger pixelBytesPerRow = bitsPerPixel > 0 && pixelWidth > 0 ? (pixelWidth * bitsPerPixel + 7) / 8 : 0;
        if (!pixels || [bitmap isPlanar] || bytesPerRow <= 0 || pixelBytesPerRow <= 0 || pixelBytesPerRow > bytesPerRow
            || pixelHeight <= 0)
        {
          std::fprintf(stderr,
                       "macos scenario capture: invalid bitmap layout"
                       " (pixels=%s planar=%d row=%ld pixel-row=%ld size=%ldx%ld bpp=%ld)\n",
                       pixels ? "present" : "missing",
                       [bitmap isPlanar] ? 1 : 0,
                       (long)bytesPerRow,
                       (long)pixelBytesPerRow,
                       (long)pixelWidth,
                       (long)pixelHeight,
                       (long)bitsPerPixel);
          [pool drain];
          return false;
        }

        unsigned long long hash = 1469598103934665603ULL;
        for (NSInteger y = 0; y < pixelHeight; ++y)
        {
          const unsigned char *row = pixels + y * bytesPerRow;
          for (NSInteger x = 0; x < pixelBytesPerRow; ++x)
          {
            hash ^= row[x];
            hash *= 1099511628211ULL;
          }
        }

        NSData *png = [bitmap representationUsingType:LOKA_MAC_BITMAP_PNG_FILE_TYPE
                                           properties:[NSDictionary dictionary]];
        NSString *outputPath = [NSString stringWithUTF8String:path];
        const bool wrote = png && outputPath && [png writeToFile:outputPath atomically:YES];
        if (!wrote)
        {
          std::fprintf(stderr,
                       "macos scenario capture: PNG encode/write failed (png=%s path=%s)\n",
                       png ? "present" : "missing",
                       outputPath ? "present" : "missing");
        }
        if (wrote)
        {
          outHash = hash;
          outPixelWidth = static_cast<long>(pixelWidth);
          outPixelHeight = static_cast<long>(pixelHeight);
        }
        [pool drain];
        return wrote;
      }

      bool WriteProfile(const char *path, Window &window, long pixelWidth, long pixelHeight)
      {
        if (!path || !*path)
        {
          return false;
        }
        std::string osBuild;
        struct utsname machine;
        const bool hasMachine = uname(&machine) == 0;
        int scalePercent = 0;
        int depth = 0;
        Window::DisplayAppearance appearance = Window::DISPLAY_APPEARANCE_LIGHT;
        const bool hasScale = window.queryDisplayScalePercent(scalePercent);
        const bool hasDepth = window.queryDisplayDepth(depth);
        const bool hasAppearance = window.queryDisplayAppearance(appearance);
        if (!ReadSysctlString("kern.osversion", osBuild) || !hasMachine)
        {
          return false;
        }
        typedef scenario_tests::ProfileFact<int> IntFact;
        typedef scenario_tests::ProfileFact<std::string> StringFact;
        const IntFact scaleFact = hasScale ? IntFact::available(scalePercent) : IntFact::unavailable();
        const IntFact depthFact = hasDepth ? IntFact::available(depth) : IntFact::unavailable();
        const StringFact appearanceFact =
            hasAppearance ? StringFact::available(appearance == Window::DISPLAY_APPEARANCE_DARK ? "dark" : "light")
                          : StringFact::unavailable();
        const scenario_tests::ScenarioProfile profile(osBuild,
                                                      machine.machine,
                                                      scaleFact,
                                                      depthFact,
                                                      appearanceFact,
                                                      "NSView.cacheDisplayInRect.v1",
                                                      pixelWidth,
                                                      pixelHeight);
        return WriteFile(path, profile.render());
      }

      bool PublishMarker(const char *path, const char *content)
      {
        if (!path || !*path || !content)
        {
          return false;
        }
        const std::string temporary = std::string(path) + ".tmp";
        FILE *output = std::fopen(temporary.c_str(), "wb");
        if (!output)
        {
          return false;
        }
        const size_t size = std::strlen(content);
        const bool wrote = std::fwrite(content, 1, size, output) == size;
        const bool closed = std::fclose(output) == 0;
        if (!wrote || !closed)
        {
          return false;
        }
        return std::rename(temporary.c_str(), path) == 0;
      }

      bool FileExists(const std::string &path)
      {
        FILE *input = std::fopen(path.c_str(), "rb");
        if (!input)
        {
          return false;
        }
        std::fclose(input);
        return true;
      }
    } // namespace

    class ScenarioRunState::Impl
    {
    public:
      enum Phase
      {
        PHASE_DRIVING,
        PHASE_SETTLING,
        PHASE_HOLDING,
        PHASE_FINISHED
      };

      Impl(const dsl::SnapTestConfig::Settings &settings, ScenarioRunMode mode)
          : settings_(settings),
            audit_(ArtifactFile(settings, kActualAudit), settings.scenario.c_str()),
            mode_(mode),
            phase_(PHASE_DRIVING),
            tick_(0),
            settleFrames_(0),
            hasPreviousHash_(false),
            previousHash_(0)
      {
      }

      void tick(Window *window, App *app, scenario_tests::ScenarioDriver &driver)
      {
        ++this->tick_;
        switch (this->phase_)
        {
        case PHASE_DRIVING:
          this->drive(window, app, driver);
          return;
        case PHASE_SETTLING:
          this->settle(window, app);
          return;
        case PHASE_HOLDING:
          this->hold(app);
          return;
        case PHASE_FINISHED:
          return;
        }
      }

      dsl::testing::ScenarioAuditSink *audit()
      {
        return &this->audit_;
      }

    private:
      void finish(App *app)
      {
        this->phase_ = PHASE_FINISHED;
        if (app)
        {
          app->quit();
        }
      }

      void fail(const char *message, App *app)
      {
        std::fprintf(stderr, "macos scenario: %s\n", message ? message : "failed");
        this->phase_ = PHASE_FINISHED;
        if (app)
        {
          app->quit();
        }
      }

      void drive(Window *window, App *app, scenario_tests::ScenarioDriver &driver)
      {
        const loka::core::Frame frame = window ? window->frameState().get() : loka::core::Frame();
        scenario_tests::CaptureContentBounds bounds;
        if (frame.width > 0 && frame.height > 0)
        {
          bounds.available = true;
          bounds.right = frame.width;
          bounds.bottom = frame.height;
        }
        dsl::SnapRecord record;
        const scenario_tests::ScenarioAdvance advance = driver.step(this->tick_, window, bounds, record);
        switch (advance)
        {
        case scenario_tests::SCENARIO_ADVANCE_PENDING:
          return;
        case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
          return;
        case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
          break;
        }
        (void)driver.publishVerdict(record);
        if (!this->audit_.isValid())
        {
          this->fail("could not write actual.audit", app);
          return;
        }
        this->phase_ = PHASE_SETTLING;
      }

      void hold(App *app)
      {
        const std::string releasePath = ArtifactPath(this->settings_, kReleaseMarker);
        if (!FileExists(releasePath))
        {
          return;
        }
        const std::string completionPath = ArtifactPath(this->settings_, kCompletionMarker);
        if (!PublishMarker(completionPath.c_str(), "artifacts-ready\n"))
        {
          this->fail("could not publish completion after inspect release", app);
          return;
        }
        this->finish(app);
      }

      void settle(Window *window, App *app)
      {
        MacWindow *macWindow = window ? window->asMacWindow() : 0;
        if (!macWindow)
        {
          this->fail("MacWindow was unavailable during settle", app);
          return;
        }
        ++this->settleFrames_;
        const char *frameName = (this->settleFrames_ % 2) == 0 ? "settle-b.png" : "settle-a.png";
        const std::string framePath = ArtifactPath(this->settings_, frameName);
        unsigned long long hash = 0;
        long pixelWidth = 0;
        long pixelHeight = 0;
        if (!CaptureContentFrame(*macWindow, framePath.c_str(), hash, pixelWidth, pixelHeight))
        {
          if (this->settleFrames_ >= kMaximumSettleFrames)
          {
            this->fail("settle timeout while content capture remained unavailable", app);
          }
          return;
        }
        if (this->hasPreviousHash_ && hash == this->previousHash_)
        {
          const std::string actualPath = ArtifactPath(this->settings_, kActualImage);
          const std::string profilePath = ArtifactPath(this->settings_, kActualProfile);
          if (!CopyFile(framePath.c_str(), actualPath.c_str())
              || !WriteProfile(profilePath.c_str(), *window, pixelWidth, pixelHeight))
          {
            this->fail("could not publish settled artifacts", app);
            return;
          }
          if (this->mode_ == SCENARIO_RUN_MODE_INSPECT)
          {
            const std::string readyPath = ArtifactPath(this->settings_, kReadyMarker);
            if (!PublishMarker(readyPath.c_str(), "inspection-ready\n"))
            {
              this->fail("could not publish inspect ready marker", app);
              return;
            }
            this->phase_ = PHASE_HOLDING;
            return;
          }
          const std::string completionPath = ArtifactPath(this->settings_, kCompletionMarker);
          if (!PublishMarker(completionPath.c_str(), "artifacts-ready\n"))
          {
            this->fail("could not publish completion", app);
            return;
          }
          this->finish(app);
          return;
        }
        this->hasPreviousHash_ = true;
        this->previousHash_ = hash;
        if (this->settleFrames_ >= kMaximumSettleFrames)
        {
          this->fail("settle timeout", app);
        }
      }

      const dsl::SnapTestConfig::Settings settings_;
      dsl::testing::ScenarioAuditFile audit_;
      const ScenarioRunMode mode_;
      Phase phase_;
      long tick_;
      long settleFrames_;
      bool hasPreviousHash_;
      unsigned long long previousHash_;
    };

    ScenarioRunState::ScenarioRunState(const dsl::SnapTestConfig::Settings &settings, ScenarioRunMode mode)
        : impl_(new(std::nothrow) Impl(settings, mode))
    {
      assert(this->impl_.get() && "ScenarioRunState storage is required");
    }

    ScenarioRunState::~ScenarioRunState() {}

    dsl::testing::ScenarioAuditSink *ScenarioRunState::audit()
    {
      return this->impl_.get() ? this->impl_->audit() : 0;
    }

    void ScenarioRunState::tick(Window *window, App *app, scenario_tests::ScenarioDriver &driver)
    {
      if (this->impl_.get())
      {
        this->impl_->tick(window, app, driver);
      }
      else if (app)
      {
        app->quit();
      }
    }

    bool LoadScenarioSettings(dsl::SnapTestConfig::Settings &settings, ScenarioRunMode &mode)
    {
      const bool configLoaded = dsl::SnapTestConfig::load(kConfigPath, settings);
      if (!configLoaded || !settings.hasScenario || !settings.hasCaptureDir || !ResolveRunMode(mode))
      {
        std::fprintf(stderr, "macos scenario: LokaTest.cfg is missing or invalid\n");
        return false;
      }
      return true;
    }

    int RunScenarioMain(ScenarioApplicationMain applicationMain)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      const int result = applicationMain ? applicationMain() : 2;
      // Draining the outermost process-lifetime pool crashes during shutdown
      // (hosted-CI SIGSEGV on exactly this line). The process is exiting here
      // anyway, so let the OS reclaim it, as every other macOS entry point in
      // this repository does.
      (void)pool;
      return result;
    }
  } // namespace macos_scenario_tests
} // namespace loka
