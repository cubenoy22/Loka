#include "platform/null/NullScenePlatformController.hpp"

#include <cassert>

#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "platform/null/context/NullButtonContext.hpp"
#include "platform/null/context/NullEditTextContext.hpp"
#include "platform/null/context/NullScrollBarContext.hpp"
#include "app/scene/projection/PlatformApplyPlan.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "platform/null/context/NullTextContext.hpp"

class NullScenePlatformController::LayoutTraversal
    : public loka::app::scene::IPlatformLayoutTraversal
{
public:
  explicit LayoutTraversal(NullScenePlatformController *controller)
      : controller_(controller),
        resultY_(0)
  {
  }

  virtual int layoutChild(loka::app::scene::Node *child,
                          const loka::app::scene::LayoutState &state)
  {
    if (!this->controller_)
    {
      return state.y;
    }
    const int result = this->controller_->layoutNode(child, state);
    this->resultY_ = static_cast<short>(result);
    return result;
  }

  virtual void setLayoutResultY(short y)
  {
    this->resultY_ = y;
  }

  virtual short layoutResultY() const
  {
    return this->resultY_;
  }

private:
  NullScenePlatformController *controller_;
  short resultY_;
};

NullScenePlatformController::RefusedProjectedNodeHandlers::RefusedProjectedNodeHandlers()
    : cell_(loka::app::scene::NodeTypeToken<loka::app::CellNode>()),
      popupMenu_(loka::app::scene::NodeTypeToken<loka::app::PopupMenuNode>()),
      imageView_(loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>()),
      openFileDialog_(loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>())
{
}

void NullScenePlatformController::RefusedProjectedNodeHandlers::registerWith(
    NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&this->cell_);
  controller.registerNodeHandler(&this->popupMenu_);
  controller.registerNodeHandler(&this->imageView_);
  controller.registerNodeHandler(&this->openFileDialog_);
}

unsigned NullScenePlatformController::RefusedProjectedNodeHandlers::cellCount() const
{
  return this->cell_.refusalCount();
}

NullScenePlatformController::NullScenePlatformController(std::size_t bucketDepthCap)
    : layoutHandlers_(),
      refusedProjectedNodeHandlers_(),
      nodeHandlers_(),
      rootNode_(0),
      ledger_(),
      lastOnChangeFlags_(loka::app::scene::NODE_DIRTY_NONE),
      onChangeCallCount_(0),
      retired_(),
      allHandles_(),
      buttonBucket_(bucketDepthCap),
      editTextBucket_(bucketDepthCap),
      scrollBarBucket_(bucketDepthCap),
      teardownCounters_(),
      intakeCheckFailCount_(0),
      createdCount_(0),
      disposedCount_(0),
      injectedDeliveryCount_(0),
      nextEventSequence_(1),
      nextHandleId_(1),
      preserveNextRetiredOwner_(false),
      skipNextProjection_(false),
      destroyed_(false),
      eventLog_()
{
  loka::app::layout::RowLayoutMetrics rowMetrics;
  rowMetrics.gap = 4;
  rowMetrics.fallbackHeight = 10;
  rowMetrics.buttonHeight = 10;
  rowMetrics.editTextHeight = 10;
  rowMetrics.popupMenuHeight = 10;
  rowMetrics.textHeight = 10;
  rowMetrics.imageFallbackHeight = 10;
  loka::app::layout::GridLayoutMetrics gridMetrics;
  gridMetrics.gapX = 2;
  gridMetrics.gapY = 4;
  loka::app::layout::RegisterBuiltinPlatformLayoutHandlers(
      this->layoutHandlers_, &rowMetrics, &gridMetrics);
  RegisterNullButtonNodeHandler(*this);
  RegisterNullEditTextNodeHandler(*this);
  RegisterNullScrollBarNodeHandler(*this);
  RegisterNullTextNodeHandler(*this);
  this->refusedProjectedNodeHandlers_.registerWith(*this);
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
  (void)fullRebuild;
  this->lastOnChangeFlags_ = flags;
  ++this->onChangeCallCount_;
  this->rootNode_ = rootNode;

  if (this->skipNextProjection_)
  {
    this->skipNextProjection_ = false;
    return;
  }
  loka::app::scene::LayoutState state;
  state.width = 100;
  state.height = 20;
  state.lineHeight = 20;
  this->layoutNode(rootNode, state);
}

namespace
{
  void syncScrollBarsInSubtree(loka::app::scene::Node *node)
  {
    if (!node)
    {
      return;
    }
    if (node->kind() == loka::app::scene::NODE_KIND_SCROLL_BAR && node->getContext())
    {
      static_cast<NullScrollBarContext *>(node->getContext())->syncFromNode();
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    if (!nestable)
    {
      return;
    }
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(
        nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      syncScrollBarsInSubtree(child);
    }
  }
} // namespace

void NullScenePlatformController::onBoundaryApply(loka::app::scene::Node *rootNode,
                                                  loka::app::scene::BoundaryNode *boundary,
                                                  const loka::app::scene::BoundaryLocalApplyInfo &info,
                                                  const loka::app::scene::PlatformApplyPlan &plan)
{
  (void)rootNode;
  if (!boundary || !plan.hasBoundaryApplyWork(boundary) || !info.hasPaintWork())
  {
    return;
  }
  syncScrollBarsInSubtree(static_cast<loka::app::scene::Node *>(boundary));
}

void NullScenePlatformController::synchronize() {}

void NullScenePlatformController::drainNativeRetirements()
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
  if (!handler)
  {
    assert(false && "no node handler registered for this node type -- register the handler or an explicit RefusedNodeHandler");
    return false;
  }
  return handler->ensureContext(node, this, state) != 0;
}

bool NullScenePlatformController::registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler)
{
  return this->nodeHandlers_.registerHandler(handler);
}

int NullScenePlatformController::projectLayoutForTesting(
    loka::app::scene::Node *node,
    const loka::app::scene::LayoutState &state)
{
  return this->layoutNode(node, state);
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

unsigned NullScenePlatformController::cellRefusalCount() const
{
  return this->refusedProjectedNodeHandlers_.cellCount();
}

loka::app::scene::ExactMatchHandleBucket<NullScenePlatformController::FakeControlHandle *> &
NullScenePlatformController::bucketFor(ControlRecipe recipe)
{
  if (recipe == CONTROL_RECIPE_BUTTON)
  {
    return this->buttonBucket_;
  }
  if (recipe == CONTROL_RECIPE_SCROLL_BAR)
  {
    return this->scrollBarBucket_;
  }
  return this->editTextBucket_;
}

const loka::app::scene::ExactMatchHandleBucket<NullScenePlatformController::FakeControlHandle *> &
NullScenePlatformController::bucketFor(ControlRecipe recipe) const
{
  return const_cast<NullScenePlatformController *>(this)->bucketFor(recipe);
}

NullScenePlatformController::BucketStats
NullScenePlatformController::bucketStats(ControlRecipe recipe) const
{
  const loka::app::scene::ExactMatchHandleBucket<FakeControlHandle *> &bucket = this->bucketFor(recipe);
  return BucketStats(bucket.hitCount(), bucket.missCount(), bucket.evictCount(), bucket.depth());
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

bool NullScenePlatformController::hasHitTarget(ControlRecipe recipe) const
{
  const LedgerRow *row = this->findLedgerRow(recipe);
  return row && row->handle && row->handle->hitOwner;
}

bool NullScenePlatformController::injectNotification(ControlRecipe recipe)
{
  if (!this->hasHitTarget(recipe))
  {
    return false;
  }
  ++this->injectedDeliveryCount_;
  return true;
}

unsigned long NullScenePlatformController::injectedDeliveryCount() const
{
  return this->injectedDeliveryCount_;
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
  const bool acquired = this->bucketFor(recipe).tryAcquire(handle);
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
  handle->hitOwner = owner;
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
  if (handle->hitOwner)
  {
    handle->hitOwner = 0;
    ++this->teardownCounters_.hitRouteRemoved;
  }

  this->retired_.push_back(RetiredEntry(handle, recipe, hint));
  ++this->teardownCounters_.queuedForNativeRetirement;
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

int NullScenePlatformController::layoutNode(loka::app::scene::Node *node,
                                            const loka::app::scene::LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }

  loka::app::scene::IPlatformLayoutHandler *layoutHandler = this->layoutHandlers_.find(node);
  if (layoutHandler)
  {
    LayoutTraversal traversal(this);
    return layoutHandler->layoutNode(node, state, &traversal);
  }

  if (loka::app::scene::IProjectedLayoutNode *projected = node->asProjectedLayoutNode())
  {
    loka::app::scene::LayoutState projectedState = state;
    return projected->layoutProjected(this, projectedState);
  }

  loka::app::scene::INestable *nestable = node->asNestable();
  if (!nestable)
  {
    return state.y;
  }
  loka::app::scene::LayoutState childState = state;
  for (loka::app::scene::Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
  {
    childState.y = static_cast<short>(this->layoutNode(child, childState));
  }
  return childState.y;
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
    for (std::size_t rowIndex = 0; rowIndex < this->ledger_.size(); ++rowIndex)
    {
      if (this->ledger_[rowIndex].handle == handle)
      {
        this->ledger_.erase(this->ledger_.begin() + rowIndex);
        ++this->teardownCounters_.ledgerRowRemovedAtSafePoint;
        break;
      }
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
    const bool accepted = this->bucketFor(entry.recipe).offer(handle);
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
  this->scrollBarBucket_.drainWith(dispose);
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
