#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>

#include <cassert>
#include <cstdio>
#include <new>
#include <string>

#include "MacWindow.hpp"
#include "MainNode.hpp"
#include "ScrapbookScenarios.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/MacWindowTestAccess.hpp"

namespace loka
{
  namespace macos_scenario_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";
      const char *kActualRecord = "actual.snap";
      const char *kActualImage = "actual.png";
      const char *kActualProfile = "actual.profile";
      const char *kCompletionMarker = "complete";
      const long kMaximumSettleFrames = 120;

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

      bool CaptureContentFrame(MacWindow &window,
                               const char *path,
                               unsigned long long &outHash,
                               long &outPixelWidth,
                               long &outPixelHeight)
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
        const NSRect viewFrame = [view frame];
        NSWindow *nativeWindow = (NSWindow *)dsl::testing::MacWindowTestAccess::nativeWindow(window);
        const NSRect windowFrame = nativeWindow ? [nativeWindow frame] : NSZeroRect;
        const NSRect contentRect = nativeWindow ? [nativeWindow contentRectForFrameRect:windowFrame] : NSZeroRect;
        std::fprintf(stderr,
                     "macos scenario capture geometry: bounds=(%.3f,%.3f %.3fx%.3f)"
                     " view-frame=(%.3f,%.3f %.3fx%.3f) window-frame=%.3fx%.3f content=%.3fx%.3f\n",
                     bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height, viewFrame.origin.x,
                     viewFrame.origin.y, viewFrame.size.width, viewFrame.size.height, windowFrame.size.width,
                     windowFrame.size.height, contentRect.size.width, contentRect.size.height);
        NSBitmapImageRep *bitmap = [view bitmapImageRepForCachingDisplayInRect:bounds];
        if (!bitmap)
        {
          std::fprintf(stderr,
                       "macos scenario capture: bitmapImageRepForCachingDisplayInRect returned nil"
                       " (bounds %.0fx%.0f, window=%s)\n",
                       bounds.size.width, bounds.size.height, [view window] ? "present" : "missing");
          [pool drain];
          return false;
        }
        [view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
        unsigned char *pixels = [bitmap bitmapData];
        const NSInteger bytesPerRow = [bitmap bytesPerRow];
        const NSInteger bitsPerPixel = [bitmap bitsPerPixel];
        const NSInteger pixelWidth = [bitmap pixelsWide];
        const NSInteger pixelHeight = [bitmap pixelsHigh];
        const NSInteger pixelBytesPerRow =
            bitsPerPixel > 0 && pixelWidth > 0 ? (pixelWidth * bitsPerPixel + 7) / 8 : 0;
        if (!pixels || [bitmap isPlanar] || bytesPerRow <= 0 || pixelBytesPerRow <= 0
            || pixelBytesPerRow > bytesPerRow || pixelHeight <= 0)
        {
          std::fprintf(stderr,
                       "macos scenario capture: invalid bitmap layout"
                       " (pixels=%s planar=%d row=%ld pixel-row=%ld size=%ldx%ld bpp=%ld)\n",
                       pixels ? "present" : "missing", [bitmap isPlanar] ? 1 : 0, (long)bytesPerRow,
                       (long)pixelBytesPerRow, (long)pixelWidth, (long)pixelHeight, (long)bitsPerPixel);
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

        NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG
                                           properties:[NSDictionary dictionary]];
        NSString *outputPath = [NSString stringWithUTF8String:path];
        const bool wrote = png && outputPath && [png writeToFile:outputPath atomically:YES];
        if (!wrote)
        {
          std::fprintf(stderr, "macos scenario capture: PNG encode/write failed (png=%s path=%s)\n",
                       png ? "present" : "missing", outputPath ? "present" : "missing");
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
        if (!ReadSysctlString("kern.osversion", osBuild) || !hasMachine || !hasScale || !hasDepth || !hasAppearance)
        {
          return false;
        }

        FILE *output = std::fopen(path, "wb");
        if (!output)
        {
          return false;
        }
        const int result = std::fprintf(output,
                                        "profile_version=1\n"
                                        "os_build=%s\n"
                                        "arch=%s\n"
                                        "scale_percent=%d\n"
                                        "depth=%d\n"
                                        "appearance=%s\n"
                                        "capture_api=NSView.cacheDisplayInRect.v1\n"
                                        "pixel_width=%ld\n"
                                        "pixel_height=%ld\n",
                                        osBuild.c_str(), machine.machine, scalePercent, depth,
                                        appearance == Window::DISPLAY_APPEARANCE_DARK ? "dark" : "light", pixelWidth,
                                        pixelHeight);
        const bool ok = result > 0 && std::fclose(output) == 0;
        return ok;
      }

      bool PublishCompletion(const char *path)
      {
        if (!path || !*path)
        {
          return false;
        }
        const std::string temporary = std::string(path) + ".tmp";
        FILE *output = std::fopen(temporary.c_str(), "wb");
        if (!output)
        {
          return false;
        }
        const char marker[] = "artifacts-ready\n";
        const bool wrote = std::fwrite(marker, 1, sizeof(marker) - 1, output) == sizeof(marker) - 1;
        const bool closed = std::fclose(output) == 0;
        if (!wrote || !closed)
        {
          return false;
        }
        return std::rename(temporary.c_str(), path) == 0;
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
            *this->observed_ = node ? static_cast<scrapbook::MainNode *>(node) : 0;
          }
          return node;
        }

      private:
        scrapbook::MainNode **observed_;
      };

      class ScenarioRunState
      {
      public:
        enum Phase
        {
          PHASE_DRIVING,
          PHASE_SETTLING,
          PHASE_ARTIFACTS_READY,
          PHASE_FINISHED
        };

        explicit ScenarioRunState(const dsl::SnapTestConfig::Settings &settings)
            : settings_(settings),
              scenario_(settings.scenario),
              phase_(PHASE_DRIVING),
              tick_(0),
              settleFrames_(0),
              hasPreviousHash_(false),
              previousHash_(0)
        {
        }

        void tick(Window *window, scrapbook::MainNode *mainNode, App *app)
        {
          ++this->tick_;
          switch (this->phase_)
          {
          case PHASE_DRIVING:
            this->drive(window, mainNode, app);
            return;
          case PHASE_SETTLING:
            this->settle(window, app);
            return;
          case PHASE_ARTIFACTS_READY:
            this->finish(app);
            return;
          case PHASE_FINISHED:
            return;
          }
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

        void drive(Window *window, scrapbook::MainNode *mainNode, App *app)
        {
          if (!window || !mainNode)
          {
            this->fail("window or MainNode was not mounted", app);
            return;
          }
          const loka::core::Frame frame = window->frameState().get();
          scenario_tests::CaptureContentBounds bounds;
          if (frame.width > 0 && frame.height > 0)
          {
            bounds.available = true;
            bounds.right = frame.width;
            bounds.bottom = frame.height;
          }
          dsl::SnapRecord record;
          if (!this->scenario_.step(this->tick_, *mainNode, bounds, record))
          {
            return;
          }
          const std::string recordPath = ArtifactPath(this->settings_, kActualRecord);
          if (dsl::SnapFileWriter::appendRecordStatus(recordPath.c_str(), record) != dsl::SNAP_WRITE_OK)
          {
            this->fail("could not write actual.snap", app);
            return;
          }
          this->phase_ = PHASE_SETTLING;
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
            const std::string completionPath = ArtifactPath(this->settings_, kCompletionMarker);
            if (!CopyFile(framePath.c_str(), actualPath.c_str())
                || !WriteProfile(profilePath.c_str(), *window, pixelWidth, pixelHeight)
                || !PublishCompletion(completionPath.c_str()))
            {
              this->fail("could not publish settled artifacts", app);
              return;
            }
            this->phase_ = PHASE_ARTIFACTS_READY;
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
        scenario_tests::ScrapbookScenario scenario_;
        Phase phase_;
        long tick_;
        long settleFrames_;
        bool hasPreviousHash_;
        unsigned long long previousHash_;
      };

      class ScenarioAppConfig : public AppConfigurable
      {
      public:
        ScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : AppConfigurable(context),
              runState_(settings),
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
                                       .title("LokaScrapbookScenarioMacOS")
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
            self->runState_.tick(window, self->borrowedMainNode_, self->borrowedApp_);
          }
        }

        ScenarioRunState runState_;
        App *borrowedApp_;
        scrapbook::MainNode *borrowedMainNode_;
      };
    } // namespace

    int RunScenarioApplication()
    {
      dsl::SnapTestConfig::Settings settings;
      if (!dsl::SnapTestConfig::load(kConfigPath, settings) || !settings.hasScenario
          || !settings.hasCaptureDir || !scenario_tests::IsRegisteredScenario(settings.scenario))
      {
        std::fprintf(stderr, "macos scenario: LokaTest.cfg is missing or invalid\n");
        return 2;
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
  } // namespace macos_scenario_tests
} // namespace loka

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  const int result = loka::macos_scenario_tests::RunScenarioApplication();
  (void)pool;
  return result;
}
