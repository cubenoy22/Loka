#ifndef LOKA_TESTS_PLATFORM_NULL_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TESTS_PLATFORM_NULL_SCENE_PLATFORM_CONTROLLER_HPP

#include <cstddef>
#include <vector>

#include "app/RectSurface.hpp"
#include "app/scene/projection/NativeHandlePool.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/projection/PlatformLayoutHandler.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "app/scene/projection/ProjectionParentScope.hpp"

class NullButtonContext;
class NullEditTextContext;
class NullScrollBarContext;
class NullWindow;

class NullScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  enum ControlRecipe
  {
    CONTROL_RECIPE_BUTTON,
    CONTROL_RECIPE_EDIT_TEXT,
    CONTROL_RECIPE_SCROLL_BAR
  };

  struct FakeControlHandle
  {
    FakeControlHandle(int identity, loka::app::scene::NodeContext *contextOwner)
        : id(identity),
          owner(contextOwner),
          hitOwner(contextOwner),
          disposed(false),
          leakedDeliberately(false)
    {
    }

    int id;
    loka::app::scene::NodeContext *owner;
    loka::app::scene::NodeContext *hitOwner;
    bool disposed;
    bool leakedDeliberately;
  };

  struct LedgerRow
  {
    LedgerRow(FakeControlHandle *nativeHandle,
              ControlRecipe controlRecipe,
              loka::app::scene::NativeLifetimeHint lifetimeHint)
        : handle(nativeHandle),
          visible(false),
          hint(lifetimeHint),
          recipe(controlRecipe)
    {
    }

    FakeControlHandle *handle;
    bool visible;
    loka::app::scene::NativeLifetimeHint hint;
    ControlRecipe recipe;
  };

  enum EventKind
  {
    EVENT_CONTROL_CREATED,
    EVENT_CONTROL_SHOWN,
    EVENT_CONTROL_HIDDEN,
    EVENT_CONTROL_DISPOSED,
    EVENT_FLUSH_BEGIN,
    EVENT_FLUSH_END,
    EVENT_DRAIN_BEGIN,
    EVENT_DRAIN_END,
    EVENT_WINDOW_DISPOSED
  };

  struct EventRecord
  {
    EventRecord(unsigned long eventSequence, EventKind eventKind, int handleIdentity)
        : sequence(eventSequence),
          kind(eventKind),
          handleId(handleIdentity)
    {
    }

    unsigned long sequence;
    EventKind kind;
    int handleId;
  };

  struct TeardownCounters
  {
    TeardownCounters()
        : backPointerCleared(0),
          hitRouteRemoved(0),
          queuedForNativeRetirement(0),
          ledgerRowRemovedAtSafePoint(0)
    {
    }

    unsigned long backPointerCleared;
    unsigned long hitRouteRemoved;
    unsigned long queuedForNativeRetirement;
    unsigned long ledgerRowRemovedAtSafePoint;
  };

  struct BucketStats
  {
    BucketStats(unsigned long hits, unsigned long misses, unsigned long evicts, std::size_t liveDepth)
        : hitCount(hits),
          missCount(misses),
          evictCount(evicts),
          depth(liveDepth)
    {
    }

    unsigned long hitCount;
    unsigned long missCount;
    unsigned long evictCount;
    std::size_t depth;
  };

  explicit NullScenePlatformController(std::size_t bucketDepthCap = 8);
  virtual ~NullScenePlatformController();

  virtual void onChange(loka::app::scene::Node *rootNode,
                        loka::app::scene::NodeDirtyFlags flags,
                        bool fullRebuild);
  /** Real platforms (Win32, Toolbox, macOS) opt into skipping the global
      onChange for applies that claim to be boundary-local paint-only. The
      wall contract must exercise the same skip, or an apply that lies about
      its structure work presents correctly here while every real platform
      goes dark (#277). */
  virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const
  {
    return true;
  }
  virtual void synchronize();
  /** The null arm's paint channel. Toolbox re-applies scroll-bar props at
      draw (ensureScrollBarControl inside ToolboxScrollBarContext::draw), so
      a paint-only apply legitimately refreshes displayed values there. The
      wall contract needs the same channel or value/range flows that ride
      paint on real platforms are only observable through the projection
      sweep here. */
  virtual void onBoundaryApply(loka::app::scene::Node *rootNode,
                               loka::app::scene::BoundaryNode *boundary,
                               const loka::app::scene::BoundaryLocalApplyInfo &info,
                               const loka::app::scene::PlatformApplyPlan &plan);
  virtual bool hasPendingSync() const;
  virtual void drainNativeRetirements();
  virtual void destroy();
  virtual bool prepareProjectedLayout(loka::app::scene::Node *node,
                                      loka::app::scene::LayoutState &state);
  virtual bool registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler);

  /** Runs the same deterministic projection traversal as onChange with
      caller-supplied bounds so geometry contracts can use fixed fixtures. */
  int projectLayoutForTesting(loka::app::scene::Node *node,
                              const loka::app::scene::LayoutState &state);

  const std::vector<LedgerRow> &ledger() const;
  loka::app::scene::NodeDirtyFlags lastOnChangeFlags() const
  {
    return lastOnChangeFlags_;
  }
  unsigned long onChangeCallCount() const
  {
    return onChangeCallCount_;
  }
  const std::vector<FakeControlHandle *> &allHandles() const;
  const std::vector<EventRecord> &eventLog() const;
  std::size_t retiredCount() const;
  const TeardownCounters &teardownCounters() const;
  unsigned long intakeCheckFailCount() const;
  unsigned long createdCount() const;
  unsigned long disposedCount() const;
  unsigned cellRefusalCount() const;
  unsigned scrollViewShortRangeRefusalCount() const;
  unsigned nestedScrollViewRefusalCount() const;
  unsigned projectionParentScopeDepthForTesting() const;
  BucketStats bucketStats(ControlRecipe recipe) const;
  unsigned long eventCount(EventKind kind) const;
  const LedgerRow *findLedgerRow(ControlRecipe recipe) const;
  bool hasHitTarget(ControlRecipe recipe) const;
  bool injectNotification(ControlRecipe recipe);
  unsigned long injectedDeliveryCount() const;

  /** Test injection: leaves the next retiring handle's owner set so the
      intake consistency check can exercise its deliberate-release arm. */
  void preserveNextRetiredOwnerForTesting();

  /** Test injection: materializes the logical tree without projecting native
      contexts during the next apply. */
  void skipNextProjectionForTesting();

private:
  class LayoutTraversal;

  class RefusedProjectedNodeHandlers
  {
  public:
    RefusedProjectedNodeHandlers();
    void registerWith(NullScenePlatformController &controller);
    unsigned cellCount() const;

  private:
    loka::app::scene::RefusedNodeHandler cell_;
    loka::app::scene::RefusedNodeHandler popupMenu_;
    loka::app::scene::RefusedNodeHandler imageView_;
    loka::app::scene::RefusedNodeHandler openFileDialog_;
  };

  struct RetiredEntry
  {
    RetiredEntry(FakeControlHandle *nativeHandle,
                 ControlRecipe controlRecipe,
                 loka::app::scene::NativeLifetimeHint lifetimeHint)
        : handle(nativeHandle),
          recipe(controlRecipe),
          hint(lifetimeHint)
    {
    }

    FakeControlHandle *handle;
    ControlRecipe recipe;
    loka::app::scene::NativeLifetimeHint hint;
  };

  class DisposePooledHandle
  {
  public:
    explicit DisposePooledHandle(NullScenePlatformController *controller)
        : controller_(controller)
    {
    }

    void operator()(FakeControlHandle *handle) const;

  private:
    NullScenePlatformController *controller_;
  };

  /** The single place that maps a recipe to its pool bucket. Adding a
      recipe without a bucket here is a compile-visible omission instead of
      a control that silently pools with the wrong kind. */
  loka::app::scene::ExactMatchHandleBucket<FakeControlHandle *> &bucketFor(ControlRecipe recipe);
  const loka::app::scene::ExactMatchHandleBucket<FakeControlHandle *> &bucketFor(ControlRecipe recipe) const;

  FakeControlHandle *createLedgerRow(ControlRecipe recipe,
                                     loka::app::scene::NodeContext *owner,
                                     loka::app::scene::NativeLifetimeHint hint);
  void completeContextTeardown(FakeControlHandle *handle);
  void setVisible(FakeControlHandle *handle, bool visible);
  void observeHint(FakeControlHandle *handle, loka::app::scene::NativeLifetimeHint hint);
  LedgerRow *findLedgerRow(FakeControlHandle *handle);
  int layoutNode(loka::app::scene::Node *node,
                 const loka::app::scene::LayoutState &state);
  int layoutScrollView(loka::app::scene::Node *node,
                       const loka::app::scene::LayoutState &state);
  int projectLayout(loka::app::scene::Node *node,
                    const loka::app::scene::LayoutState &state);
  void refuseScrollViewShortRange();
  void refuseNestedScrollView();
  bool refuseNarrowingInScrollScope(int resultY);
  void flushRetired();
  void drainBuckets();
  void disposeHandle(FakeControlHandle *handle);
  void appendEvent(EventKind kind, int handleId);
  void recordWindowDisposed();

  loka::app::scene::PlatformLayoutHandlerRegistry layoutHandlers_;
  RefusedProjectedNodeHandlers refusedProjectedNodeHandlers_;
  loka::app::scene::PlatformNodeHandlerRegistry nodeHandlers_;
  loka::app::RectSurfaceExtentLedger rectSurfaceExtentLedger_;
  loka::app::scene::Node *rootNode_;
  std::vector<LedgerRow> ledger_;
  loka::app::scene::NodeDirtyFlags lastOnChangeFlags_;
  unsigned long onChangeCallCount_;
  std::vector<RetiredEntry> retired_;
  std::vector<FakeControlHandle *> allHandles_;
  loka::app::scene::ExactMatchHandleBucket<FakeControlHandle *> buttonBucket_;
  loka::app::scene::ExactMatchHandleBucket<FakeControlHandle *> editTextBucket_;
  loka::app::scene::ExactMatchHandleBucket<FakeControlHandle *> scrollBarBucket_;
  TeardownCounters teardownCounters_;
  unsigned long intakeCheckFailCount_;
  unsigned long createdCount_;
  unsigned long disposedCount_;
  unsigned long injectedDeliveryCount_;
  unsigned long nextEventSequence_;
  int nextHandleId_;
  loka::app::scene::ProjectionParentScopeStack projectionParentScopes_;
  unsigned scrollViewShortRangeRefusalCount_;
  unsigned nestedScrollViewRefusalCount_;
  bool preserveNextRetiredOwner_;
  bool skipNextProjection_;
  bool destroyed_;
  std::vector<EventRecord> eventLog_;

  NullScenePlatformController(const NullScenePlatformController &);
  NullScenePlatformController &operator=(const NullScenePlatformController &);

  friend class NullButtonContext;
  friend class NullEditTextContext;
  friend class NullScrollBarContext;
  friend class NullWindow;
};

#endif // LOKA_TESTS_PLATFORM_NULL_SCENE_PLATFORM_CONTROLLER_HPP
