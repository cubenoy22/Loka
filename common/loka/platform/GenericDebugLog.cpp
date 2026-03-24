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
                                  int transactionEmpty,
                                  unsigned int childCount,
                                  unsigned int firstChildKind,
                                  const char *firstChildTestId)
    {
      std::fprintf(stderr,
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
