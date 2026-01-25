#include "loka/core/Profiler.hpp"

namespace loka
{
  namespace core
  {
    ProfilerBackend *gProfilerBackend = 0;
    ProfileEntry gProfileData[32];
    int gProfileCount = 0;

    // Function-level profiler storage
    FuncProfileSlot *gProfileSlots[kMaxProfileSlots];
    int gProfileSlotCount = 0;
  } // namespace core
} // namespace loka
