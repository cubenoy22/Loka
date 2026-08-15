#ifndef LOKA_TESTS_SCENARIOS_SCENARIO_TYPES_HPP
#define LOKA_TESTS_SCENARIOS_SCENARIO_TYPES_HPP

#include "testing/snap/SnapFormat.hpp"

namespace loka
{
  namespace scenario_tests
  {
    enum ScenarioCompletionPolicy
    {
      SCENARIO_COMPLETION_DRIVER_OWNED = 0,
      SCENARIO_COMPLETION_HOLD_FINAL_SCENE
    };

    enum ScenarioAdvance
    {
      SCENARIO_ADVANCE_PENDING = 0,
      SCENARIO_ADVANCE_DRIVER_COMPLETION_READY,
      SCENARIO_ADVANCE_FINAL_SCENE_HELD
    };

    /** Half-open content rectangle in the captured content's local coordinates. */
    struct CaptureContentBounds
    {
      CaptureContentBounds()
          : available(false),
            left(0),
            top(0),
            right(0),
            bottom(0)
      {
      }

      bool available;
      long left;
      long top;
      long right;
      long bottom;
    };

    /** Writes one platform-neutral content crop into a scenario record. */
    void SetContentBounds(dsl::SnapRecord &record, const CaptureContentBounds &bounds);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENARIO_TYPES_HPP
