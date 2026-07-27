#include "core/Profiler.hpp"

#if !LOKA_PROFILE_FUNC_TICKS
#error "ProfilerCompileTest requires LOKA_PROFILE_FUNC_TICKS=1"
#endif

void compileProfileSectionsInOneScope()
{
  PROFILE_SECTION("first");
  PROFILE_SECTION("second");
}
