#ifndef LOKA_TESTS_PLATFORM_NULL_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TESTS_PLATFORM_NULL_SCENE_PLATFORM_CONTROLLER_HPP

#include <cstddef>
#include <vector>

#include "app/scene/projection/NativeHandlePool.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"

class NullButtonContext;
class NullEditTextContext;
class NullWindow;

class NullScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  enum ControlRecipe
  {
    CONTROL_RECIPE_BUTTON,
    CONTROL_RECIPE_EDIT_TEXT
  };

  struct FakeControlHandle
  {
    FakeControlHandle(int identity, loka::app::scene::NodeContext *contextOwner)
        : id(identity),
          owner(contextOwner),
          disposed(false),
          leakedDeliberately(false)
    {
    }

    int id;
    loka::app::scene::NodeContext *owner;
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
          rowRemoved(0),
          handedToPool(0)
    {
    }

    unsigned long backPointerCleared;
    unsigned long rowRemoved;
    unsigned long handedToPool;
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
  virtual void synchronize();
  virtual bool hasPendingSync() const;
  virtual void destroy();
  virtual bool prepareProjectedLayout(loka::app::scene::Node *node,
                                      loka::app::scene::LayoutState &state);
  virtual bool registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler);

  const std::vector<LedgerRow> &ledger() const;
  const std::vector<FakeControlHandle *> &allHandles() const;
  const std::vector<EventRecord> &eventLog() const;
  std::size_t retiredCount() const;
  const TeardownCounters &teardownCounters() const;
  unsigned long intakeCheckFailCount() const;
  unsigned long createdCount() const;
  unsigned long disposedCount() const;
  BucketStats bucketStats(ControlRecipe recipe) const;
  unsigned long eventCount(EventKind kind) const;
  const LedgerRow *findLedgerRow(ControlRecipe recipe) const;

  /** Test injection: leaves the next retiring handle's owner set so the
      intake consistency check can exercise its deliberate-release arm. */
  void preserveNextRetiredOwnerForTesting();

  /** Test injection: materializes the logical tree without projecting native
      contexts during the next apply. */
  void skipNextProjectionForTesting();

private:
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

  FakeControlHandle *createLedgerRow(ControlRecipe recipe,
                                     loka::app::scene::NodeContext *owner,
                                     loka::app::scene::NativeLifetimeHint hint);
  void completeContextTeardown(FakeControlHandle *handle);
  void setVisible(FakeControlHandle *handle, bool visible);
  void observeHint(FakeControlHandle *handle, loka::app::scene::NativeLifetimeHint hint);
  LedgerRow *findLedgerRow(FakeControlHandle *handle);
  void projectNode(loka::app::scene::Node *node);
  void flushRetired();
  void drainBuckets();
  void disposeHandle(FakeControlHandle *handle);
  void appendEvent(EventKind kind, int handleId);
  void recordWindowDisposed();

  loka::app::scene::PlatformNodeHandlerRegistry nodeHandlers_;
  loka::app::scene::Node *rootNode_;
  std::vector<LedgerRow> ledger_;
  std::vector<RetiredEntry> retired_;
  std::vector<FakeControlHandle *> allHandles_;
  loka::app::scene::ExactMatchHandleBucket<FakeControlHandle *> buttonBucket_;
  loka::app::scene::ExactMatchHandleBucket<FakeControlHandle *> editTextBucket_;
  TeardownCounters teardownCounters_;
  unsigned long intakeCheckFailCount_;
  unsigned long createdCount_;
  unsigned long disposedCount_;
  unsigned long nextEventSequence_;
  int nextHandleId_;
  bool preserveNextRetiredOwner_;
  bool skipNextProjection_;
  bool destroyed_;
  std::vector<EventRecord> eventLog_;

  NullScenePlatformController(const NullScenePlatformController &);
  NullScenePlatformController &operator=(const NullScenePlatformController &);

  friend class NullButtonContext;
  friend class NullEditTextContext;
  friend class NullWindow;
};

#endif // LOKA_TESTS_PLATFORM_NULL_SCENE_PLATFORM_CONTROLLER_HPP
