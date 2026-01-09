#include <windows.h>
#include <cstdio>
#include "loka/platform/DebugLog.hpp"

namespace loka
{
  namespace platform
  {
    void DebugLogRecomposeTracked(void *boundary, void *scene)
    {
      char buffer[128];
      std::sprintf(buffer, "[recompose] update tracked, queue scene refresh (boundary=%p scene=%p)\n", boundary, scene);
      OutputDebugStringA(buffer);
    }

    void DebugLogRecomposeQueued(void *scene)
    {
      char buffer[96];
      std::sprintf(buffer, "[recompose] queued update task (scene=%p)\n", scene);
      OutputDebugStringA(buffer);
    }
  } // namespace platform
} // namespace loka
