#include "Profiler.hpp"

namespace declara
{
  namespace core
  {
    ProfilerBackend *gProfilerBackend = 0;
    ProfileEntry gProfileData[32];
    int gProfileCount = 0;

    // Cumulative timing for compose phases
    long gComposeAttachTicks = 0;
    long gComposeNodeTicks = 0;
    long gComposeCreateTicks = 0;

    // Cumulative timing for composeTree breakdown
    long gTreeVirtTicks = 0;
    long gTreeCtxTicks = 0;
    long gTreeCompTicks = 0;
    int gTreeNodeCount = 0;
  } // namespace core
} // namespace declara
