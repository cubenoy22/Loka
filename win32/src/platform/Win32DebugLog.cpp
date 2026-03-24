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
  } // namespace platform
} // namespace loka
