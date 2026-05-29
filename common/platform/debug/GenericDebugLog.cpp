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

    void DebugLogSceneFlags(void *scene,
                           const char *stage,
                           unsigned int flags,
                           unsigned int boundaryFlags,
                           int fullRebuild)
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

    void DebugLogSceneDecision(void *scene,
                               int requiresStructure,
                               int requiresLayout,
                               int canApplyLocalDiff)
    {
      std::fprintf(stderr,
                   "[scene-decision] scene=%p structure=%d layout=%d localDiff=%d\n",
                   scene,
                   requiresStructure,
                   requiresLayout,
                   canApplyLocalDiff);
      std::fflush(stderr);
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
      std::fprintf(stderr,
                   "[scene-structure-root] scene=%p boundary=%p pendingFlags=0x%X composed=%d hasDiff=%d empty=%d compatibleRetain=%d requires=%d\n",
                   scene,
                   boundary,
                   pendingDirtyFlags,
                   composed,
                   hasDiff,
                   emptyDiff,
                   compatibleRetainOnly,
                   requiresStructure);
      std::fflush(stderr);
    }

    void DebugLogSceneRootDiffDecision(void *scene,
                                      void *boundary,
                                      unsigned int dirtyFlagsSeen,
                                      int composed,
                                      int preservedNativeContexts)
    {
      std::fprintf(stderr,
                   "[scene-root-diff] scene=%p boundary=%p dirtyFlagsSeen=0x%X composed=%d preserved=%d\n",
                   scene,
                   boundary,
                   dirtyFlagsSeen,
                   composed,
                   preservedNativeContexts);
      std::fflush(stderr);
    }

    void DebugLogSceneRootDiffShape(void *scene,
                                   void *boundary,
                                   int entryCount,
                                   int hasIncompatibleRetain,
                                   int compatibleRetainOnly,
                                   int stableRetainOnly)
    {
      std::fprintf(stderr,
                   "[scene-root-diff-shape] scene=%p boundary=%p entries=%d incompatible=%d compatibleRetainOnly=%d stableRetainOnly=%d\n",
                   scene,
                   boundary,
                   entryCount,
                   hasIncompatibleRetain,
                   compatibleRetainOnly,
                   stableRetainOnly);
      std::fflush(stderr);
    }

    void DebugLogSceneRootIdentity(void *scene,
                                  void *boundary,
                                  unsigned int kind,
                                  const char *testId,
                                  int hasPreviousSnapshotRoot,
                                  int hasCurrentSnapshotRoot,
                                  int compositionDiffStateEmpty,
                                  unsigned int childCount,
                                  unsigned int firstChildKind,
                                  const char *firstChildTestId)
    {
      std::fprintf(stderr,
                   "[scene-root-id] scene=%p boundary=%p kind=%u testId=%s prevSnap=%d currSnap=%d diffStateEmpty=%d childCount=%u firstChildKind=%u firstChildTestId=%s\n",
                   scene,
                   boundary,
                   kind,
                   testId ? testId : "",
                   hasPreviousSnapshotRoot,
                   hasCurrentSnapshotRoot,
                   compositionDiffStateEmpty,
                   childCount,
                   firstChildKind,
                   firstChildTestId ? firstChildTestId : "");
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
