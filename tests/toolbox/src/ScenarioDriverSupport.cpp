#include "ScenarioDriverSupport.hpp"

#include "ToolboxWindow.hpp"
#include "app/core/Window.hpp"
#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kAuditFile = "LokaTestsToolbox.audit";
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

    HostCompletionSignal::HostCompletionSignal()
        : window_(0)
    {
    }

    HostCompletionSignal::~HostCompletionSignal()
    {
      if (this->window_)
      {
        DisposeWindow(this->window_);
        this->window_ = 0;
      }
    }

    bool HostCompletionSignal::publish()
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
      SetRect(&bounds,
              static_cast<short>(bounds.right - 12),
              static_cast<short>(bounds.bottom - 12),
              static_cast<short>(bounds.right - 4),
              static_cast<short>(bounds.bottom - 4));
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
  } // namespace toolbox_tests
} // namespace loka
