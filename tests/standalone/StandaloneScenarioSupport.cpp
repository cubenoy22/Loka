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

    StandaloneRunControl::StandaloneRunControl(const char *applicationName, std::FILE *diagnostics)
        : borrowedApp_(0),
          applicationName_(applicationName ? applicationName : "application"),
          diagnostics_(diagnostics ? diagnostics : stderr),
          tick_(0),
          mountFailed_(false)
    {
    }

    void StandaloneRunControl::setApp(App *app)
    {
      this->borrowedApp_ = app;
#if LOKA_STANDALONE_PERFORMANCE_RUNS > 0
      if (app)
      {
        this->scenarioVerdict_.begin();
      }
#endif
    }

    StandaloneRunControl::Advance StandaloneRunControl::advance(bool mainNodeMounted)
    {
      if (this->mountFailed_)
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

      this->mountFailed_ = true;
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

    void StandaloneRunControl::observeScenarioAdvance(scenario_tests::ScenarioAdvance advance,
                                                      const dsl::SnapRecord &record)
    {
      switch (advance)
      {
      case scenario_tests::SCENARIO_ADVANCE_PENDING:
      case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
        return;
      case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
        break;
      }
#if LOKA_STANDALONE_PERFORMANCE_RUNS > 0
      this->scenarioVerdict_.observe(record);
      if (this->borrowedApp_)
      {
        this->borrowedApp_->quit();
      }
#else
      (void)record;
#endif
    }

    long StandaloneRunControl::tick() const
    {
      return this->tick_;
    }

    bool StandaloneRunControl::failed() const
    {
#if LOKA_STANDALONE_PERFORMANCE_RUNS > 0
      return this->mountFailed_ || this->scenarioVerdict_.refusesCompletedPass();
#else
      return this->mountFailed_;
#endif
    }
  } // namespace standalone_tests
} // namespace loka
