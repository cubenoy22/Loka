#ifndef LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_SUPPORT_HPP
#define LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_SUPPORT_HPP

#include <Quickdraw.h>

#include "ScenarioTypes.hpp"
#include "platform/file/FileHandle.hpp"

class Window;

namespace loka
{
  namespace dsl
  {
    class SnapRecord;
    class SnapTestConfig;
  } // namespace dsl

  namespace toolbox_tests
  {
    scenario_tests::CaptureContentBounds QueryCaptureContentBounds(Window *window);
    scenario_tests::CaptureContentBounds ContentLocalBounds(const scenario_tests::CaptureContentBounds &screenBounds);

    platform::file::FileHandle ResolveScenarioAuditFile();
    bool WriteScenarioErrorAudit(const char *scenario, const dsl::SnapRecord &record);

    /** Owns the native marker used by MAME's live-screen completion seam. */
    class HostCompletionSignal
    {
    public:
      HostCompletionSignal();
      ~HostCompletionSignal();
      bool publish();

    private:
      HostCompletionSignal(const HostCompletionSignal &);
      HostCompletionSignal &operator=(const HostCompletionSignal &);
      WindowPtr window_;
    };
  } // namespace toolbox_tests
} // namespace loka

#endif // LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_SUPPORT_HPP
