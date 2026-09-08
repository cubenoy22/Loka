#if defined(LOKA_DEBUG_RECOMPOSE)
#include <windows.h>
#include <cstdio>
#include "platform/debug/DebugLog.hpp"

namespace loka
{
  namespace platform
  {
    void DebugLogRecomposeTracked(void *boundary, void *scene)
    {
      char buffer[128];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[recompose] update tracked, queue scene refresh (boundary=%p scene=%p)\n",
                 boundary,
                 scene);
      OutputDebugStringA(buffer);
    }

    void DebugLogRecomposeQueued(void *scene)
    {
      char buffer[96];
      ::snprintf(buffer, sizeof(buffer), "[recompose] queued update task (scene=%p)\n", scene);
      OutputDebugStringA(buffer);
    }

    void DebugLogRecomposeMerged(void *scene)
    {
      char buffer[112];
      ::snprintf(buffer, sizeof(buffer), "[recompose] merged into pending update task (scene=%p)\n", scene);
      OutputDebugStringA(buffer);
    }

    void
    DebugLogSceneFlags(void *scene, const char *stage, unsigned int flags, unsigned int boundaryFlags, int fullRebuild)
    {
      char buffer[192];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[scene-flags] %s scene=%p flags=0x%X boundaryFlags=0x%X fullRebuild=%d\n",
                 stage ? stage : "?",
                 scene,
                 flags,
                 boundaryFlags,
                 fullRebuild);
      OutputDebugStringA(buffer);
    }

    void DebugLogSceneDecision(void *scene, int requiresStructure, int requiresLayout)
    {
      char buffer[160];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[scene-decision] scene=%p structure=%d layout=%d\n",
                 scene,
                 requiresStructure,
                 requiresLayout);
      OutputDebugStringA(buffer);
    }

    void DebugLogBoundaryComposeDispatch(void *boundary,
                                         unsigned int eventValue,
                                         unsigned int dirtyFlags,
                                         int isRootBoundary)
    {
      char buffer[192];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[boundary-compose] boundary=%p event=%u dirtyFlags=0x%X root=%d\n",
                 boundary,
                 eventValue,
                 dirtyFlags,
                 isRootBoundary);
      OutputDebugStringA(buffer);
    }
  } // namespace platform
} // namespace loka
#endif
