#ifndef LOKA_PLATFORM_DEBUG_LOG_HPP
#define LOKA_PLATFORM_DEBUG_LOG_HPP

namespace loka
{
  namespace platform
  {
#if defined(LOKA_DEBUG_SCENE_UPDATE)
    void DebugLogSceneUpdateTracked(void *boundary, void *scene);
    void DebugLogSceneUpdateQueued(void *scene);
    void DebugLogSceneUpdateMerged(void *scene);
    void
    DebugLogSceneFlags(void *scene, const char *stage, unsigned int flags, unsigned int boundaryFlags, int fullRebuild);
    void DebugLogSceneDecision(void *scene, int requiresStructure, int requiresLayout);
    void DebugLogBoundaryComposeDispatch(void *boundary,
                                         unsigned int eventValue,
                                         unsigned int dirtyFlags,
                                         int isRootBoundary);
#else
    inline void DebugLogSceneUpdateTracked(void *, void *) {}
    inline void DebugLogSceneUpdateQueued(void *) {}
    inline void DebugLogSceneUpdateMerged(void *) {}
    inline void DebugLogSceneFlags(void *, const char *, unsigned int, unsigned int, int) {}
    inline void DebugLogSceneDecision(void *, int, int) {}
    inline void DebugLogBoundaryComposeDispatch(void *, unsigned int, unsigned int, int) {}
#endif
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_DEBUG_LOG_HPP
