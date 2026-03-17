#include "loka/platform/DebugLog.hpp"
#include <cstdio>

namespace loka
{
  namespace platform
  {
    void DebugLogRecomposeTracked(void *boundary, void *scene)
    {
      std::fprintf(stderr, "[recompose] update tracked, queue scene refresh (boundary=%p scene=%p)\n", boundary, scene);
      std::fflush(stderr);
    }

    void DebugLogRecomposeQueued(void *scene)
    {
      std::fprintf(stderr, "[recompose] queued update task (scene=%p)\n", scene);
      std::fflush(stderr);
    }

    void DebugLogRecomposeMerged(void *scene)
    {
      std::fprintf(stderr, "[recompose] merged into pending update task (scene=%p)\n", scene);
      std::fflush(stderr);
    }
  } // namespace platform
} // namespace loka
