#include "ScenarioProfile.hpp"

#include <sstream>

namespace loka
{
  namespace scenario_tests
  {
    ScenarioProfile::ScenarioProfile(const std::string &osBuild,
                                     const std::string &architecture,
                                     const ProfileFact<int> &scalePercent,
                                     const ProfileFact<int> &depth,
                                     const ProfileFact<std::string> &appearance,
                                     const std::string &captureApi,
                                     long pixelWidth,
                                     long pixelHeight)
        : osBuild_(osBuild),
          architecture_(architecture),
          scalePercent_(scalePercent),
          depth_(depth),
          appearance_(appearance),
          captureApi_(captureApi),
          pixelWidth_(pixelWidth),
          pixelHeight_(pixelHeight)
    {
    }

    std::string ScenarioProfile::render() const
    {
      int scalePercent = 0;
      int depth = 0;
      std::string appearance;
      const bool hasScalePercent = this->scalePercent_.query(scalePercent);
      const bool hasDepth = this->depth_.query(depth);
      const bool hasAppearance = this->appearance_.query(appearance);
      std::ostringstream output;
      output << "profile_version=2\n"
             << "os_build=" << this->osBuild_ << "\n"
             << "arch=" << this->architecture_ << "\n"
             << "scale_percent_available=" << (hasScalePercent ? 1 : 0) << "\n";
      if (hasScalePercent)
      {
        output << "scale_percent=" << scalePercent << "\n";
      }
      output << "depth_available=" << (hasDepth ? 1 : 0) << "\n";
      if (hasDepth)
      {
        output << "depth=" << depth << "\n";
      }
      output << "appearance_available=" << (hasAppearance ? 1 : 0) << "\n";
      if (hasAppearance)
      {
        output << "appearance=" << appearance << "\n";
      }
      output << "capture_api=" << this->captureApi_ << "\n"
             << "pixel_width=" << this->pixelWidth_ << "\n"
             << "pixel_height=" << this->pixelHeight_ << "\n";
      return output.str();
    }
  } // namespace scenario_tests
} // namespace loka
