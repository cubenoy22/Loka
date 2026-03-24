#ifndef LOKA_PLATFORM_DEBUG_LOG_HPP
#define LOKA_PLATFORM_DEBUG_LOG_HPP

namespace loka
{
  namespace platform
  {
    void DebugLogRecomposeTracked(void *boundary, void *scene);
    void DebugLogRecomposeQueued(void *scene);
    void DebugLogRecomposeMerged(void *scene);
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
                                  int transactionEmpty,
                                  unsigned int childCount,
                                  unsigned int firstChildKind,
                                  const char *firstChildTestId);
    void DebugLogBoundaryComposeDispatch(void *boundary,
                                        unsigned int eventValue,
                                        unsigned int dirtyFlags,
                                        int isRootBoundary);
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_DEBUG_LOG_HPP
