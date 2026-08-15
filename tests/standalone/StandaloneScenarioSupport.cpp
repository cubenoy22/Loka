#include "StandaloneScenarioSupport.hpp"

#include <cstdio>

#include "app/core/App.hpp"
#include "app/core/Window.hpp"
#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"

namespace loka
{
  namespace standalone_tests
  {
    scenario_tests::CaptureContentBounds StandaloneContentBounds(Window *window)
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

    platform::file::FileHandle ResolveStandaloneAuditFile()
    {
      platform::file::FileHandle result;
      if (!platform::file::ResolveApplicationSidecar(file::File::Application() << file::File("LOG.TXT"), result))
      {
        return platform::file::FileHandle();
      }
      return result;
    }

    StandaloneMountDeadline::StandaloneMountDeadline(const char *applicationName, std::FILE *diagnostics)
        : borrowedApp_(0),
          applicationName_(applicationName ? applicationName : "application"),
          diagnostics_(diagnostics ? diagnostics : stderr),
          tick_(0),
          failed_(false)
    {
    }

    void StandaloneMountDeadline::setApp(App *app)
    {
      this->borrowedApp_ = app;
    }

    StandaloneMountDeadline::Advance StandaloneMountDeadline::advance(bool mainNodeMounted)
    {
      if (this->failed_)
      {
        return ADVANCE_FAILED;
      }
      ++this->tick_;
      if (mainNodeMounted)
      {
        return ADVANCE_MOUNTED;
      }
      if (this->tick_ < MOUNT_DEADLINE_TICKS)
      {
        return ADVANCE_WAITING;
      }

      this->failed_ = true;
      std::fprintf(this->diagnostics_,
                   "Loka %s standalone startup failed: "
                   "MainNode was not mounted after %d idle ticks.\n",
                   this->applicationName_,
                   MOUNT_DEADLINE_TICKS);
      std::fflush(this->diagnostics_);
      if (this->borrowedApp_)
      {
        this->borrowedApp_->quit();
      }
      return ADVANCE_FAILED;
    }

    long StandaloneMountDeadline::tick() const
    {
      return this->tick_;
    }

    bool StandaloneMountDeadline::failed() const
    {
      return this->failed_;
    }
  } // namespace standalone_tests
} // namespace loka
