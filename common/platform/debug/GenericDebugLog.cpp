#include "platform/debug/DebugLog.hpp"
#include <cstdio>

#if !defined(_WIN32) && defined(LOKA_DEBUG_SCENE_UPDATE)
namespace loka
{
  namespace platform
  {
    void DebugLogSceneUpdateTracked(void *boundary, void *scene)
    {
      std::fprintf(stderr, "[scene-update] tracked dirty boundary=%p scene=%p\n", boundary, scene);
      std::fflush(stderr);
    }

    void DebugLogSceneUpdateQueued(void *scene)
    {
      std::fprintf(stderr, "[scene-update] queued update task scene=%p\n", scene);
      std::fflush(stderr);
    }

    void DebugLogSceneUpdateMerged(void *scene)
    {
      std::fprintf(stderr, "[scene-update] merged into pending update task scene=%p\n", scene);
      std::fflush(stderr);
    }

    void
    DebugLogSceneFlags(void *scene, const char *stage, unsigned int flags, unsigned int boundaryFlags, int fullRebuild)
    {
      std::fprintf(stderr,
                   "[scene-flags] %s scene=%p flags=0x%X boundaryFlags=0x%X fullRebuild=%d\n",
                   stage ? stage : "?",
                   scene,
                   flags,
                   boundaryFlags,
                   fullRebuild);
      std::fflush(stderr);
    }

    void DebugLogSceneDecision(void *scene, int requiresStructure, int requiresLayout)
    {
      std::fprintf(stderr,
                   "[scene-decision] scene=%p structure=%d layout=%d\n",
                   scene,
                   requiresStructure,
                   requiresLayout);
      std::fflush(stderr);
    }

    void DebugLogBoundaryComposeDispatch(void *boundary,
                                         unsigned int eventValue,
                                         unsigned int dirtyFlags,
                                         int isRootBoundary)
    {
      std::fprintf(stderr,
                   "[boundary-compose] boundary=%p event=%u dirtyFlags=0x%X root=%d\n",
                   boundary,
                   eventValue,
                   dirtyFlags,
                   isRootBoundary);
      std::fflush(stderr);
    }
  } // namespace platform
} // namespace loka
#endif
