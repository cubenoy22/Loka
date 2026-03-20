#ifndef LOKA_TOOLBOX_SCENE_DEBUG_STATS_HPP
#define LOKA_TOOLBOX_SCENE_DEBUG_STATS_HPP

#include "app/scene/Node.hpp"
#include <string>

class ToolboxSceneDebugStats
{
public:
  ToolboxSceneDebugStats();

  static std::string flagsToString(loka::app::scene::NodeDirtyFlags flags);
  void begin(loka::app::scene::NodeDirtyFlags flags, bool fullRebuild);
  void refreshHitCounts(int buttonHits, int cellHits, int editHits, int textHits, int popupHits);
  std::string summary() const;
  void reset();
  bool dumpToTimestampedFile() const;

  unsigned long changeSequence;
  unsigned long totalChanges;
  loka::app::scene::NodeDirtyFlags lastFlags;
  bool lastFullRebuild;
  bool lastRootPresent;
  int fullInvalidateRequests;
  int rectInvalidateRequests;
  int drawCalls;
  int drawDirtyCalls;
  int renderCalls;
  int renderDirtyCalls;
  int buttonHitCount;
  int cellHitCount;
  int editHitCount;
  int textHitCount;
  int popupHitCount;
  int controlDrawCount;
  int relayoutTextCount;
  int textChangedCellCount;
  int textChangedTextCount;
  int textChangedEditHitCount;
  int textChangedEditControlCount;
  int textChangedPendingCount;
  int textChangedImmediateInvalidateCount;
  int batchOnChangeCount;
  int batchNullRootCount;
  int batchFullRebuildCount;
  int batchNonNoneFlagsCount;
  int batchAccumOnChangeCount;
  int batchAccumNullRootCount;
  bool batchAccumFullRebuild;
  loka::app::scene::NodeDirtyFlags batchAccumFlags;
  std::string batchAccumTrace;
  std::string relayoutTextPreview;
  bool fallbackRootIsBoundary;
  bool fallbackRootHasLayoutBounds;
  bool fallbackQueuedByChild;
  bool fallbackUsedFullInvalidate;
  int windowFullRequestCount;
  int windowRectRequestCount;
  int windowFlushFullCount;
  int windowFlushDirtyCount;
  int windowUpdateEvtDrawCount;
  const char *windowFullRequestSource;
  int requestInvalidateCallCount;
  bool requestInvalidateFirstRootPresent;
  bool requestInvalidateFirstFullRebuild;
  loka::app::scene::NodeDirtyFlags requestInvalidateFirstFlags;
  bool requestInvalidateRootPresent;
  bool requestInvalidateFullRebuild;
  loka::app::scene::NodeDirtyFlags requestInvalidateFlags;
  int totalFullInvalidateRequests;
  int totalRectInvalidateRequests;
  int totalDrawCalls;
  int totalDrawDirtyCalls;
  int totalRenderCalls;
  int totalRenderDirtyCalls;
  int totalControlDrawCount;
};

#endif // LOKA_TOOLBOX_SCENE_DEBUG_STATS_HPP
