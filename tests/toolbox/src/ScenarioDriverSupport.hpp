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

    /** Publishes the completion-time native capture geometry and then the
        marker used by MAME's live-screen completion seam. */
    class ScenarioCompletionPublisher
    {
    public:
      ScenarioCompletionPublisher();
      ~ScenarioCompletionPublisher();
      bool publish(Window *window);

    private:
      ScenarioCompletionPublisher(const ScenarioCompletionPublisher &);
      ScenarioCompletionPublisher &operator=(const ScenarioCompletionPublisher &);
      bool publishHostSignal();
      WindowPtr signalWindow_;
    };
  } // namespace toolbox_tests
} // namespace loka

#endif // LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_SUPPORT_HPP
