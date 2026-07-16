#include "platform/null/NullScenePlatformController.hpp"

#include "platform/null/context/NullButtonContext.hpp"
#include "platform/null/context/NullEditTextContext.hpp"

NullScenePlatformController::NullScenePlatformController(std::size_t bucketDepthCap)
    : nodeHandlers_(),
      rootNode_(0),
      ledger_(),
      retired_(),
      allHandles_(),
      buttonBucket_(bucketDepthCap),
      editTextBucket_(bucketDepthCap),
      teardownCounters_(),
      intakeCheckFailCount_(0),
      createdCount_(0),
      disposedCount_(0),
      nextEventSequence_(1),
      nextHandleId_(1),
      preserveNextRetiredOwner_(false),
      skipNextProjection_(false),
      destroyed_(false),
      eventLog_()
{
  RegisterNullButtonNodeHandler(*this);
  RegisterNullEditTextNodeHandler(*this);
}

NullScenePlatformController::~NullScenePlatformController()
{
  this->destroy();
  for (std::size_t i = 0; i < this->allHandles_.size(); ++i)
  {
    delete this->allHandles_[i];
  }
  this->allHandles_.clear();
}

void NullScenePlatformController::onChange(loka::app::scene::Node *rootNode,
                                           loka::app::scene::NodeDirtyFlags flags,
                                           bool fullRebuild)
{
  (void)flags;
  (void)fullRebuild;
  this->rootNode_ = rootNode;

  // Context retirement happens before the global apply callback. Flush first
  // so the live projection can draw from the just-completed exact-match bag.
  this->appendEvent(EVENT_FLUSH_BEGIN, 0);
  this->flushRetired();
  this->appendEvent(EVENT_FLUSH_END, 0);
  if (this->skipNextProjection_)
  {
    this->skipNextProjection_ = false;
    return;
  }
  this->projectNode(rootNode);
}

void NullScenePlatformController::synchronize()
{
  if (this->retired_.empty())
  {
    return;
  }
  this->appendEvent(EVENT_FLUSH_BEGIN, 0);
  this->flushRetired();
  this->appendEvent(EVENT_FLUSH_END, 0);
}

bool NullScenePlatformController::hasPendingSync() const
{
  return !this->retired_.empty();
}

void NullScenePlatformController::destroy()
{
  if (this->destroyed_)
  {
    return;
  }
  if (this->rootNode_)
  {
    this->releaseNodeContexts(this->rootNode_);
  }
  this->appendEvent(EVENT_DRAIN_BEGIN, 0);
  this->flushRetired();
  this->drainBuckets();
  this->appendEvent(EVENT_DRAIN_END, 0);
  this->rootNode_ = 0;
  this->destroyed_ = true;
}

bool NullScenePlatformController::prepareProjectedLayout(loka::app::scene::Node *node,
                                                         loka::app::scene::LayoutState &state)
{
  if (!node)
  {
    return false;
  }
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlers_.find(node);
  return handler && handler->ensureContext(node, this, state) != 0;
}

bool NullScenePlatformController::registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler)
{
  return this->nodeHandlers_.registerHandler(handler);
}

const std::vector<NullScenePlatformController::LedgerRow> &NullScenePlatformController::ledger() const
{
  return this->ledger_;
}

const std::vector<NullScenePlatformController::FakeControlHandle *> &
NullScenePlatformController::allHandles() const
{
  return this->allHandles_;
}

const std::vector<NullScenePlatformController::EventRecord> &NullScenePlatformController::eventLog() const
{
  return this->eventLog_;
}

std::size_t NullScenePlatformController::retiredCount() const
{
  return this->retired_.size();
}

const NullScenePlatformController::TeardownCounters &NullScenePlatformController::teardownCounters() const
{
  return this->teardownCounters_;
}

unsigned long NullScenePlatformController::intakeCheckFailCount() const
{
  return this->intakeCheckFailCount_;
}

unsigned long NullScenePlatformController::createdCount() const
{
  return this->createdCount_;
}

unsigned long NullScenePlatformController::disposedCount() const
{
  return this->disposedCount_;
}

NullScenePlatformController::BucketStats
NullScenePlatformController::bucketStats(ControlRecipe recipe) const
{
  if (recipe == CONTROL_RECIPE_BUTTON)
  {
    return BucketStats(this->buttonBucket_.hitCount(),
                       this->buttonBucket_.missCount(),
                       this->buttonBucket_.evictCount(),
                       this->buttonBucket_.depth());
  }
  return BucketStats(this->editTextBucket_.hitCount(),
                     this->editTextBucket_.missCount(),
                     this->editTextBucket_.evictCount(),
                     this->editTextBucket_.depth());
}

unsigned long NullScenePlatformController::eventCount(EventKind kind) const
{
  unsigned long count = 0;
  for (std::size_t i = 0; i < this->eventLog_.size(); ++i)
  {
    if (this->eventLog_[i].kind == kind)
    {
      ++count;
    }
  }
  return count;
}

const NullScenePlatformController::LedgerRow *
NullScenePlatformController::findLedgerRow(ControlRecipe recipe) const
{
  for (std::size_t i = 0; i < this->ledger_.size(); ++i)
  {
    if (this->ledger_[i].recipe == recipe)
    {
      return &this->ledger_[i];
    }
  }
  return 0;
}

void NullScenePlatformController::preserveNextRetiredOwnerForTesting()
{
  this->preserveNextRetiredOwner_ = true;
}

void NullScenePlatformController::skipNextProjectionForTesting()
{
  this->skipNextProjection_ = true;
}

NullScenePlatformController::FakeControlHandle *
NullScenePlatformController::createLedgerRow(ControlRecipe recipe,
                                             loka::app::scene::NodeContext *owner,
                                             loka::app::scene::NativeLifetimeHint hint)
{
  FakeControlHandle *handle = 0;
  const bool acquired = recipe == CONTROL_RECIPE_BUTTON
                            ? this->buttonBucket_.tryAcquire(handle)
                            : this->editTextBucket_.tryAcquire(handle);
  if (!acquired)
  {
    handle = new FakeControlHandle(this->nextHandleId_++, owner);
    if (!handle)
    {
      return 0;
    }
    this->allHandles_.push_back(handle);
    ++this->createdCount_;
    this->appendEvent(EVENT_CONTROL_CREATED, handle->id);
  }
  handle->owner = owner;
  handle->disposed = false;
  handle->leakedDeliberately = false;
  this->ledger_.push_back(LedgerRow(handle, recipe, hint));
  return handle;
}

void NullScenePlatformController::completeContextTeardown(FakeControlHandle *handle)
{
  if (!handle)
  {
    return;
  }
  LedgerRow *row = this->findLedgerRow(handle);
  if (!row)
  {
    return;
  }
  const ControlRecipe recipe = row->recipe;
  const loka::app::scene::NativeLifetimeHint hint = row->hint;

  if (this->preserveNextRetiredOwner_)
  {
    this->preserveNextRetiredOwner_ = false;
  }
  else
  {
    handle->owner = 0;
    ++this->teardownCounters_.backPointerCleared;
  }

  for (std::size_t i = 0; i < this->ledger_.size(); ++i)
  {
    if (this->ledger_[i].handle == handle)
    {
      this->ledger_.erase(this->ledger_.begin() + i);
      ++this->teardownCounters_.rowRemoved;
      break;
    }
  }

  this->retired_.push_back(RetiredEntry(handle, recipe, hint));
  ++this->teardownCounters_.handedToPool;
}

void NullScenePlatformController::setVisible(FakeControlHandle *handle, bool visible)
{
  LedgerRow *row = this->findLedgerRow(handle);
  if (!row || row->visible == visible)
  {
    return;
  }
  row->visible = visible;
  this->appendEvent(visible ? EVENT_CONTROL_SHOWN : EVENT_CONTROL_HIDDEN, handle->id);
}

void NullScenePlatformController::observeHint(FakeControlHandle *handle,
                                              loka::app::scene::NativeLifetimeHint hint)
{
  LedgerRow *row = this->findLedgerRow(handle);
  if (row)
  {
    row->hint = hint;
  }
}

NullScenePlatformController::LedgerRow *
NullScenePlatformController::findLedgerRow(FakeControlHandle *handle)
{
  for (std::size_t i = 0; i < this->ledger_.size(); ++i)
  {
    if (this->ledger_[i].handle == handle)
    {
      return &this->ledger_[i];
    }
  }
  return 0;
}

void NullScenePlatformController::projectNode(loka::app::scene::Node *node)
{
  if (!node)
  {
    return;
  }
  loka::app::scene::LayoutState state;
  state.x = 0;
  state.y = 0;
  state.width = 100;
  state.height = 20;
  state.lineHeight = 20;
  state.spacing = 0;
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlers_.find(node);
  if (handler)
  {
    handler->ensureContext(node, this, state);
  }
  loka::app::scene::INestable *nestable = node->asNestable();
  if (!nestable)
  {
    return;
  }
  for (loka::app::scene::Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
  {
    this->projectNode(child);
  }
}

void NullScenePlatformController::flushRetired()
{
  for (std::size_t i = 0; i < this->retired_.size(); ++i)
  {
    RetiredEntry &entry = this->retired_[i];
    FakeControlHandle *handle = entry.handle;
    if (!handle)
    {
      continue;
    }
    if (entry.hint == loka::app::scene::NATIVE_HINT_EAGER_RELEASE)
    {
      this->disposeHandle(handle);
      continue;
    }
    // Bag entries must hold zero pointers into Loka; a live back-pointer
    // means the teardown sequence did not complete. Leaking the handle
    // (counted) is the safe arm — disposing it would hand the live owner
    // a dead handle, and pooling it would pay the same handle out twice.
    if (handle->owner)
    {
      ++this->intakeCheckFailCount_;
      handle->leakedDeliberately = true;
      continue;
    }
    const bool accepted = entry.recipe == CONTROL_RECIPE_BUTTON
                              ? this->buttonBucket_.offer(handle)
                              : this->editTextBucket_.offer(handle);
    if (!accepted)
    {
      this->disposeHandle(handle);
    }
  }
  this->retired_.clear();
}

void NullScenePlatformController::drainBuckets()
{
  DisposePooledHandle dispose(this);
  this->buttonBucket_.drainWith(dispose);
  this->editTextBucket_.drainWith(dispose);
}

void NullScenePlatformController::disposeHandle(FakeControlHandle *handle)
{
  if (!handle || handle->disposed)
  {
    return;
  }
  handle->owner = 0;
  handle->disposed = true;
  ++this->disposedCount_;
  this->appendEvent(EVENT_CONTROL_DISPOSED, handle->id);
}

void NullScenePlatformController::appendEvent(EventKind kind, int handleId)
{
  this->eventLog_.push_back(EventRecord(this->nextEventSequence_++, kind, handleId));
}

void NullScenePlatformController::recordWindowDisposed()
{
  this->appendEvent(EVENT_WINDOW_DISPOSED, 0);
}

void NullScenePlatformController::DisposePooledHandle::operator()(FakeControlHandle *handle) const
{
  if (this->controller_)
  {
    this->controller_->disposeHandle(handle);
  }
}
