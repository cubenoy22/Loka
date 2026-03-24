#if defined(LOKA_DEBUG_RECOMPOSE)
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
      ::snprintf(buffer, sizeof(buffer), "[recompose] update tracked, queue scene refresh (boundary=%p scene=%p)\n", boundary, scene);
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

    void DebugLogSceneFlags(void *scene,
                           const char *stage,
                           unsigned int flags,
                           unsigned int boundaryFlags,
                           int fullRebuild)
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

    void DebugLogSceneDecision(void *scene,
                               int requiresStructure,
                               int requiresLayout,
                               int canApplyLocalDiff)
    {
      char buffer[160];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[scene-decision] scene=%p structure=%d layout=%d localDiff=%d\n",
                 scene,
                 requiresStructure,
                 requiresLayout,
                 canApplyLocalDiff);
      OutputDebugStringA(buffer);
    }

    void DebugLogSceneStructureRoot(void *scene,
                                    void *boundary,
                                    unsigned int pendingDirtyFlags,
                                    int composed,
                                    int hasDiff,
                                    int emptyDiff,
                                    int compatibleRetainOnly,
                                    int requiresStructure)
    {
      char buffer[224];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[scene-structure-root] scene=%p boundary=%p pendingFlags=0x%X composed=%d hasDiff=%d empty=%d compatibleRetain=%d requires=%d\n",
                 scene,
                 boundary,
                 pendingDirtyFlags,
                 composed,
                 hasDiff,
                 emptyDiff,
                 compatibleRetainOnly,
                 requiresStructure);
      OutputDebugStringA(buffer);
    }

    void DebugLogSceneRootDiffDecision(void *scene,
                                      void *boundary,
                                      unsigned int dirtyFlagsSeen,
                                      int composed,
                                      int preservedNativeContexts)
    {
      char buffer[192];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[scene-root-diff] scene=%p boundary=%p dirtyFlagsSeen=0x%X composed=%d preserved=%d\n",
                 scene,
                 boundary,
                 dirtyFlagsSeen,
                 composed,
                 preservedNativeContexts);
      OutputDebugStringA(buffer);
    }

    void DebugLogSceneRootDiffShape(void *scene,
                                   void *boundary,
                                   int entryCount,
                                   int hasIncompatibleRetain,
                                   int compatibleRetainOnly,
                                   int stableRetainOnly)
    {
      char buffer[224];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[scene-root-diff-shape] scene=%p boundary=%p entries=%d incompatible=%d compatibleRetainOnly=%d stableRetainOnly=%d\n",
                 scene,
                 boundary,
                 entryCount,
                 hasIncompatibleRetain,
                 compatibleRetainOnly,
                 stableRetainOnly);
      OutputDebugStringA(buffer);
    }

    void DebugLogSceneRootIdentity(void *scene,
                                  void *boundary,
                                  unsigned int kind,
                                  const char *testId,
                                  int hasPreviousSnapshotRoot,
                                  int hasCurrentSnapshotRoot,
                                  int transactionEmpty,
                                  unsigned int childCount,
                                  unsigned int firstChildKind,
                                  const char *firstChildTestId)
    {
      char buffer[320];
      ::snprintf(buffer,
                 sizeof(buffer),
                 "[scene-root-id] scene=%p boundary=%p kind=%u testId=%s prevSnap=%d currSnap=%d txEmpty=%d childCount=%u firstChildKind=%u firstChildTestId=%s\n",
                 scene,
                 boundary,
                 kind,
                 testId ? testId : "",
                 hasPreviousSnapshotRoot,
                 hasCurrentSnapshotRoot,
                 transactionEmpty,
                 childCount,
                 firstChildKind,
                 firstChildTestId ? firstChildTestId : "");
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
