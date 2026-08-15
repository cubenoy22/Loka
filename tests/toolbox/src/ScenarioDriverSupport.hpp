#ifndef LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_SUPPORT_HPP
#define LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_SUPPORT_HPP

#include <Quickdraw.h>

#include "ScenarioTypes.hpp"

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

    dsl::SnapWriteStatus WriteScenarioRecord(const dsl::SnapTestConfig::Settings &settings,
                                             const dsl::SnapRecord &record);
    dsl::SnapWriteStatus WriteCaptureMetadata(const dsl::SnapTestConfig::Settings &settings,
                                              const char *test,
                                              const char *scenario,
                                              long tick,
                                              const scenario_tests::CaptureContentBounds &bounds);

    /** Owns the crop-external native marker used by MAME's live screen
        completion seam. */
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
