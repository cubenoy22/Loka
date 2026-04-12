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
    void DebugLogSceneFlags(void *scene,
                           const char *stage,
                           unsigned int flags,
                           unsigned int boundaryFlags,
                           int fullRebuild);
    void DebugLogSceneDecision(void *scene,
                               int requiresStructure,
                               int requiresLayout,
                               int canApplyLocalDiff);
    void DebugLogSceneStructureRoot(void *scene,
                                    void *boundary,
                                    unsigned int pendingDirtyFlags,
                                    int composed,
                                    int hasDiff,
                                    int emptyDiff,
                                    int compatibleRetainOnly,
                                    int requiresStructure);
    void DebugLogSceneRootDiffDecision(void *scene,
                                      void *boundary,
                                      unsigned int dirtyFlagsSeen,
                                      int composed,
                                      int preservedNativeContexts);
    void DebugLogSceneRootDiffShape(void *scene,
                                   void *boundary,
                                   int entryCount,
                                   int hasIncompatibleRetain,
                                   int compatibleRetainOnly,
                                   int stableRetainOnly);
    void DebugLogSceneRootIdentity(void *scene,
                                  void *boundary,
                                  unsigned int kind,
                                  const char *testId,
                                  int hasPreviousSnapshotRoot,
                                  int hasCurrentSnapshotRoot,
                                  int compositionDiffStateEmpty,
                                  unsigned int childCount,
                                  unsigned int firstChildKind,
                                  const char *firstChildTestId);
    void DebugLogBoundaryComposeDispatch(void *boundary,
                                        unsigned int eventValue,
                                        unsigned int dirtyFlags,
                                        int isRootBoundary);
#else
    inline void DebugLogSceneUpdateTracked(void *, void *) {}
    inline void DebugLogSceneUpdateQueued(void *) {}
    inline void DebugLogSceneUpdateMerged(void *) {}
    inline void DebugLogSceneFlags(void *, const char *, unsigned int, unsigned int, int) {}
    inline void DebugLogSceneDecision(void *, int, int, int) {}
    inline void DebugLogSceneStructureRoot(void *, void *, unsigned int, int, int, int, int, int) {}
    inline void DebugLogSceneRootDiffDecision(void *, void *, unsigned int, int, int) {}
    inline void DebugLogSceneRootDiffShape(void *, void *, int, int, int, int) {}
    inline void DebugLogSceneRootIdentity(void *,
                                          void *,
                                          unsigned int,
                                          const char *,
                                          int,
                                          int,
                                          int,
                                          unsigned int,
                                          unsigned int,
                                          const char *) {}
    inline void DebugLogBoundaryComposeDispatch(void *, unsigned int, unsigned int, int) {}
#endif
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_DEBUG_LOG_HPP
