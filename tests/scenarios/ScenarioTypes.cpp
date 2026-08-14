#include "ScenarioTypes.hpp"

namespace loka
{
  namespace scenario_tests
  {
    void SetContentBounds(dsl::SnapRecord &record, const CaptureContentBounds &bounds)
    {
      if (!bounds.available)
      {
        record.set("crop_left", "na");
        record.set("crop_top", "na");
        record.set("crop_right", "na");
        record.set("crop_bottom", "na");
        return;
      }
      record.setInt("crop_left", bounds.left);
      record.setInt("crop_top", bounds.top);
      record.setInt("crop_right", bounds.right);
      record.setInt("crop_bottom", bounds.bottom);
    }
  } // namespace scenario_tests
} // namespace loka
