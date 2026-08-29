#include "ScenarioDriverSupport.hpp"

#include <cstdio>

#include "ToolboxWindow.hpp"
#include "app/core/Window.hpp"
#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileIO.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kAuditFile = "LokaTestsToolbox.audit";
      const char *kCaptureRectangleFile = "LokaTestsToolbox.capture";

      bool QueryCaptureStructureBounds(Window *window, Rect &out)
      {
        ToolboxWindow *toolboxWindow = window ? window->asToolboxWindow() : 0;
        WindowPtr nativeWindow = toolboxWindow ? toolboxWindow->window() : 0;
        WindowPeek peek = reinterpret_cast<WindowPeek>(nativeWindow);
        if (!peek || !peek->strucRgn)
        {
          return false;
        }
        const Rect bounds = (*peek->strucRgn)->rgnBBox;
        const Rect screen = qd.screenBits.bounds;
        if (bounds.left < screen.left || bounds.top < screen.top || bounds.right > screen.right
            || bounds.bottom > screen.bottom || bounds.right <= bounds.left || bounds.bottom <= bounds.top)
        {
          return false;
        }
        out = bounds;
        return true;
      }

      bool PublishCaptureRectangle(Window *window)
      {
        Rect bounds;
        if (!QueryCaptureStructureBounds(window, bounds))
        {
          return false;
        }
        platform::file::FileHandle destination;
        if (!platform::file::ResolveApplicationSidecar(
                file::File::Application() << file::File(kCaptureRectangleFile), destination))
        {
          return false;
        }
        std::FILE *output = platform::file::OpenWriteTruncate(destination);
        if (!output)
        {
          return false;
        }
        const bool wrote = std::fprintf(output,
                                        "%d %d %d %d\n",
                                        static_cast<int>(bounds.left),
                                        static_cast<int>(bounds.top),
                                        static_cast<int>(bounds.right),
                                        static_cast<int>(bounds.bottom))
            > 0;
        const bool flushed = wrote && platform::file::FlushWrite(output, destination);
        const bool closed = std::fclose(output) == 0;
        return flushed && closed;
      }
    } // namespace

    scenario_tests::CaptureContentBounds QueryCaptureContentBounds(Window *window)
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

    scenario_tests::CaptureContentBounds ContentLocalBounds(const scenario_tests::CaptureContentBounds &screenBounds)
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

    platform::file::FileHandle ResolveScenarioAuditFile()
    {
      platform::file::FileHandle destination;
      if (!platform::file::ResolveApplicationSidecar(
              file::File::Application() << file::File(kAuditFile), destination))
      {
        return platform::file::FileHandle();
      }
      return destination;
    }

    bool WriteScenarioErrorAudit(const char *scenario, const dsl::SnapRecord &record)
    {
      dsl::testing::ScenarioAuditFile audit(ResolveScenarioAuditFile(), scenario);
      dsl::testing::scenario_audit_detail::TerminalEmitter terminal(&audit);
      return audit.isValid() && terminal.emit(dsl::testing::SCENARIO_AUDIT_FAILED, record);
    }

    ScenarioCompletionPublisher::ScenarioCompletionPublisher()
        : signalWindow_(0)
    {
    }

    ScenarioCompletionPublisher::~ScenarioCompletionPublisher()
    {
      if (this->signalWindow_)
      {
        DisposeWindow(this->signalWindow_);
        this->signalWindow_ = 0;
      }
    }

    bool ScenarioCompletionPublisher::publish(Window *window)
    {
      const bool capturePublished = PublishCaptureRectangle(window);
      const bool signalPublished = this->publishHostSignal();
      return capturePublished && signalPublished;
    }

    bool ScenarioCompletionPublisher::publishHostSignal()
    {
      if (this->signalWindow_)
      {
        return true;
      }
      Rect bounds = qd.screenBits.bounds;
      if (bounds.right - bounds.left < 16 || bounds.bottom - bounds.top < 16)
      {
        return false;
      }
      SetRect(&bounds,
              static_cast<short>(bounds.right - 12),
              static_cast<short>(bounds.bottom - 12),
              static_cast<short>(bounds.right - 4),
              static_cast<short>(bounds.bottom - 4));
      Str255 title;
      title[0] = 0;
      this->signalWindow_ = NewWindow(0, &bounds, title, true, plainDBox, reinterpret_cast<WindowPtr>(-1L), false, 0);
      if (!this->signalWindow_)
      {
        return false;
      }
      GrafPtr previousPort = 0;
      GetPort(&previousPort);
      BeginUpdate(this->signalWindow_);
      SetPort(this->signalWindow_);
      FillRect(&this->signalWindow_->portRect, &qd.black);
      EndUpdate(this->signalWindow_);
      SetPort(previousPort);
      return true;
    }
  } // namespace toolbox_tests
} // namespace loka
