#include "RectSurfaceScenarioObservation.hpp"

#include <cstdio>

#include "app/RectSurface.hpp"

namespace loka
{
  namespace scenario_tests
  {
    dsl::StepRunStatus CaptureRectSurfaceModel(app::scene::Scene *scene,
                                               const char *testId,
                                               std::string &rectangles,
                                               dsl::FlowError &error)
    {
      app::scene::Node *surfaceNode = 0;
      const dsl::StepRunStatus lookup = dsl::testing::LookupNodeById<app::scene::Node>(
          scene, testId ? testId : "", surfaceNode, error);
      if (lookup != dsl::FLOW_STEP_SUCCEEDED)
      {
        return lookup;
      }
      app::RectSurfaceNode *surface = surfaceNode ? surfaceNode->asRectSurfaceNode() : 0;
      if (!surface || !surface->props.model_)
      {
        error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO;
        error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
        return dsl::FLOW_STEP_FAILED;
      }
      const app::RectSurfaceModel &model = surface->props.model_->get();
      const short count = model.spriteCount();
      rectangles.clear();
      for (short i = 0; i < count; ++i)
      {
        if (!rectangles.empty())
        {
          rectangles += ';';
        }
        const app::RectSurfaceSprite &rect = model.sprite(i);
        char text[64];
        ::snprintf(text,
                   sizeof(text),
                   "%d,%d,%d,%d",
                   static_cast<int>(rect.x),
                   static_cast<int>(rect.y),
                   static_cast<int>(rect.width),
                   static_cast<int>(rect.height));
        rectangles += text;
      }
      return dsl::FLOW_STEP_SUCCEEDED;
    }
  } // namespace scenario_tests
} // namespace loka
