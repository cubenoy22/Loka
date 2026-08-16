#ifndef LOKA_TESTS_SCENARIOS_RECT_SURFACE_SCENARIO_OBSERVATION_HPP
#define LOKA_TESTS_SCENARIOS_RECT_SURFACE_SCENARIO_OBSERVATION_HPP

#include <string>

#include "testing/scene/SceneTestFlow.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Serializes one TEST_ID-selected RectSurface's quantized rectangles. */
    dsl::StepRunStatus CaptureRectSurfaceModel(app::scene::Scene *scene,
                                               const char *testId,
                                               std::string &rectangles,
                                               dsl::FlowError &error);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_RECT_SURFACE_SCENARIO_OBSERVATION_HPP
