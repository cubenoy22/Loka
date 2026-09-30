#include "core/StateTracker.hpp"
#include "core/State.hpp"
#include "core/Operation.hpp"
#include <cassert>
#include <cstdio>

namespace loka
{
  namespace core
  {

    namespace
    {
      void reportOperationStatus(OperationStatus status)
      {
        switch (status)
        {
        case OPERATION_SETTLED:
          break;
        case OPERATION_REFUSED_STATE_BUDGET:
          fprintf(stderr, "[Loka] StateTracker transaction did not settle before the iteration limit.\n");
          break;
        case OPERATION_REFUSED_CHAIN_LIMIT:
          fprintf(stderr, "[Loka] StateTracker commit chain did not settle before the iteration limit.\n");
          break;
        }
      }
    }

    Operation *Operation::active_ = 0;

    Operation::Operation(const OperationBudget &budget)
        : head_(0), tail_(0), budget_(budget), phase_(OPEN)
    {
      assert(!Operation::active_ && "only one Operation may be active");
      Operation::active_ = this;
    }

    Operation::~Operation()
    {
      if (this->phase_ != CLOSED)
        this->close();
    }

    OpenResult Operation::open(StateTracker *tracker)
    {
      assert(this->phase_ == OPEN || this->phase_ == DRIVING);
      assert(tracker);
      PushStateTracker *ledger = tracker->asPushTracker();
      if (!ledger)
        return OPEN_REFUSED_NOT_PUSH;
      if (ledger->op_ == this)
        return OPEN_ALREADY_OPEN;
      if (ledger->phase() != TRACKER_IDLE)
        return OPEN_REFUSED_BUSY;
      ledger->begin();
      ledger->op_ = this;
      ledger->opNext_ = 0;
      if (this->tail_)
        this->tail_->opNext_ = ledger;
      else
        this->head_ = ledger;
      this->tail_ = ledger;
      return OPEN_OK;
    }

    bool Operation::hasWork() const
    {
      for (PushStateTracker *t = this->head_; t; t = t->opNext_)
        if (t->hasWork())
          return true;
      return false;
    }

    OperationOutcome Operation::close()
    {
      assert(this->phase_ == OPEN && "Operation closes once, outside its callbacks");
      this->phase_ = DRIVING;
      OperationStatus status = OPERATION_SETTLED;
      size_t rounds = 0;
      while (this->hasWork())
      {
        if (this->budget_.rounds == 0)
        {
          status = OPERATION_REFUSED_CHAIN_LIMIT;
          break;
        }
        --this->budget_.rounds;
        PushStateTracker *const frontier = this->tail_;
        ++rounds;
        for (PushStateTracker *t = this->head_; t; t = t->opNext_)
        {
          if (t->hasWork() && t->step(this->budget_) == PushStateTracker::STEP_STATE_BUDGET)
          {
            status = OPERATION_REFUSED_STATE_BUDGET;
            break;
          }
          if (t == frontier)
            break;
        }
        if (status != OPERATION_SETTLED)
          break;
      }
      this->phase_ = CLOSING;
      for (PushStateTracker *t = this->head_; t; t = t->opNext_)
        t->removeRoutes();
      for (PushStateTracker *t = this->head_; t; t = t->opNext_)
        if (!t->drainDeferred(this->budget_) && status == OPERATION_SETTLED)
          status = OPERATION_REFUSED_CHAIN_LIMIT;
      while (this->head_)
      {
        PushStateTracker *t = this->head_;
        t->releaseClockLevel();
        this->head_ = t->opNext_;
        t->opNext_ = 0;
        t->op_ = 0;
      }
      this->tail_ = 0;
      this->phase_ = CLOSED;
      Operation::active_ = 0;
      reportOperationStatus(status);
      return OperationOutcome(status, rounds);
    }

    void Operation::withdraw(PushStateTracker *tracker)
    {
      PushStateTracker *previous = 0;
      for (PushStateTracker *t = this->head_; t; t = t->opNext_)
      {
        if (t == tracker)
        {
          if (previous)
            previous->opNext_ = t->opNext_;
          else
            this->head_ = t->opNext_;
          if (this->tail_ == t)
            this->tail_ = previous;
          t->opNext_ = 0;
          t->op_ = 0;
          return;
        }
        previous = t;
      }
    }

    PushStateTracker::PushStateTracker()
        : phase_(TRACKER_IDLE),
          pendingDirty_(false),
          depth_(0),
          reentrantDepth_(0),
          invalidateFn_(0),
          invalidateUserData_(0),
          invalidateTarget_(0),
          visitPass_(0),
          initialEntry_(),
          statesHead_(0),
          statesTail_(0),
          freeEntries_(&initialEntry_),
          chunks_(0),
          op_(0),
          opNext_(0)
    {
    }

    PushStateTracker::PushStateTracker(const std::vector<StateBase *> &states)
        : phase_(TRACKER_IDLE),
          pendingDirty_(false),
          depth_(0),
          reentrantDepth_(0),
          invalidateFn_(0),
          invalidateUserData_(0),
          invalidateTarget_(0),
          visitPass_(0),
          initialEntry_(),
          statesHead_(0),
          statesTail_(0),
          freeEntries_(&initialEntry_),
          chunks_(0),
          op_(0),
          opNext_(0)
    {
      for (size_t i = 0; i < states.size(); ++i)
      {
        addState(states[i]);
      }
    }

    PushStateTracker::~PushStateTracker()
    {
      if (this->op_)
      {
        assert(!"ledger destroyed while open by an Operation");
        this->op_->withdraw(this);
      }
      releaseEntries();
    }

    void PushStateTracker::begin()
    {
      if (depth_ > 0)
      {
        ++depth_;
        return;
      }
      if (phase_ != TRACKER_IDLE)
      {
        ++reentrantDepth_;
        return;
      }
      ++depth_;
      for (StateEntry *e = statesHead_; e; e = e->next)
        this->installRoute(e->state);
      transaction_.begin();
      phase_ = TRACKER_PRECOMMIT;
    }

    void PushStateTracker::defer(TrackerDeferKey, void (*fn)(void *), void *userData)
    {
      transaction_.intake(phase_).deferred.push_back(std::make_pair(fn, userData));
    }

    void PushStateTracker::markDirty(StateBase *state)
    {
      if (!state)
      {
        return;
      }
      // Every completed walk clears its active stamps. On unsigned wrap,
      // skip the idle sentinel; no graph-wide reset is needed.
      if (++this->visitPass_ == 0)
        ++this->visitPass_;
      this->propagateDirty(state, this->visitPass_);
    }

    void PushStateTracker::propagateDirty(StateBase *state, unsigned long pass)
    {
      TransactionIntake &intake = transaction_.intake(phase_);
      intake.dirty = true;
      transaction_.anyDirty = true;
      pendingDirty_ = true;
      DependencyMap::iterator it = dependents.find(state);
      if (it != dependents.end())
      {
        Dependents &row = it->second;
        if (row.visitPass == pass)
        {
          fprintf(stderr, "[Loka] Circular state dependency detected: StateBase %p\n", (void *)state);
          return;
        }
        row.visitPass = pass;
        for (size_t i = 0; i < row.states.size(); ++i)
        {
          this->propagateDirty(row.states[i], pass);
        }
        row.visitPass = 0;
      }
      // A walk stamp describes the active path, not intake membership across
      // separate writes. Keep the duplicate scan for both rows and leaves.
      for (size_t i = 0; i < intake.dirtyStates.size(); ++i)
      {
        if (intake.dirtyStates[i] == state)
        {
          return;
        }
      }
      intake.dirtyStates.push_back(state);
    }

    void PushStateTracker::addState(StateBase *state)
    {
      if (!state)
      {
        return;
      }
      // Check for duplicates by traversing the linked list
      for (StateEntry *e = statesHead_; e; e = e->next)
      {
        if (e->state == state)
        {
          return;
        }
      }
      // Append to linked list
      StateEntry *entry = allocateEntry(state);
      if (statesTail_)
      {
        statesTail_->next = entry;
      }
      else
      {
        statesHead_ = entry;
      }
      statesTail_ = entry;

      std::vector<StateBase *> deps = state->getDependencyStates();
      if (!deps.empty())
      {
        for (size_t j = 0; j < deps.size(); ++j)
        {
          registerDependency(state, deps[j]);
        }
      }
      if (phase_ != TRACKER_IDLE)
      {
        this->installRoute(state);
      }
    }

    void PushStateTracker::addStateUnchecked(StateBase *state)
    {
      if (!state)
      {
        return;
      }
      // Skip duplicate check - caller guarantees uniqueness
      // Append to linked list
      StateEntry *entry = allocateEntry(state);
      if (statesTail_)
      {
        statesTail_->next = entry;
      }
      else
      {
        statesHead_ = entry;
      }
      statesTail_ = entry;

      std::vector<StateBase *> deps = state->getDependencyStates();
      if (!deps.empty())
      {
        for (size_t j = 0; j < deps.size(); ++j)
        {
          registerDependency(state, deps[j]);
        }
      }
      if (phase_ != TRACKER_IDLE)
      {
        this->installRoute(state);
      }
    }

    void PushStateTracker::removeState(StateBase *state)
    {
      if (!state)
      {
        return;
      }

      StateEntry *prev = 0;
      StateEntry *entry = statesHead_;
      while (entry)
      {
        if (entry->state == state)
        {
          StateEntry *next = entry->next;
          if (prev)
          {
            prev->next = next;
          }
          else
          {
            statesHead_ = next;
          }
          if (statesTail_ == entry)
          {
            statesTail_ = prev;
          }
          entry->state = 0;
          entry->next = freeEntries_;
          freeEntries_ = entry;
          break;
        }
        prev = entry;
        entry = entry->next;
      }

      transaction_.removeState(state);
      // Preserve the active loop index when a recompute removes a later state.
      for (size_t i = 0; i < this->scratch_.size(); ++i)
      {
        if (this->scratch_[i] == state)
          this->scratch_[i] = 0;
      }

      dependents.erase(state);
      for (DependencyMap::iterator it = dependents.begin(); it != dependents.end(); ++it)
      {
        StateList &list = it->second.states;
        for (size_t i = 0; i < list.size();)
        {
          if (list[i] == state)
          {
            list.erase(list.begin() + i);
          }
          else
          {
            ++i;
          }
        }
      }
      if (state->currentTracker == this)
      {
        state->currentTracker = 0;
      }
    }

    void PushStateTracker::reserveStates(size_t count)
    {
      if (count == 0)
      {
        return;
      }
      transaction_.current.dirtyStates.reserve(count);
      transaction_.next.dirtyStates.reserve(count);
      transaction_.committedDirtyStates.reserve(count);
      this->scratch_.reserve(count);
      allocateEntries(count);
    }

    bool PushStateTracker::end()
    {
      if (depth_ == 0)
      {
        if (reentrantDepth_ > 0)
        {
          --reentrantDepth_;
        }
        return true;
      }
      --depth_;
      if (depth_ > 0)
        return true;
      assert(this->op_ == 0 && "Operation owns the final clock level");
      OperationBudget budget;
      OperationStatus status = OPERATION_SETTLED;
      while (this->hasWork() && budget.rounds > 0)
      {
        --budget.rounds;
        if (this->step(budget) == STEP_STATE_BUDGET)
        {
          status = OPERATION_REFUSED_STATE_BUDGET;
          break;
        }
      }
      if (this->hasWork() && status == OPERATION_SETTLED)
        status = OPERATION_REFUSED_CHAIN_LIMIT;
      this->removeRoutes();
      if (!this->drainDeferred(budget) && status == OPERATION_SETTLED)
        status = OPERATION_REFUSED_CHAIN_LIMIT;
      // end() consumed the legacy level above; Operation releases its level
      // only after the same route-removal and deferred-drain stages.
      reportOperationStatus(status);
      return status == OPERATION_SETTLED;
    }

    PushStateTracker::StepResult PushStateTracker::step(OperationBudget &budget)
    {
      // Rotate on entry only: the completed snapshot of the previous step
      // stays readable until the next step or the next begin().
      if (!this->transaction_.current.hasWork() && this->transaction_.next.hasWork())
        this->transaction_.advance();
      this->phase_ = TRACKER_PRECOMMIT;
      if (!this->settleCurrentTransaction(budget.stateIterations))
        return STEP_STATE_BUDGET;
      this->phase_ = TRACKER_COMMIT;
      for (size_t i = 0; i < this->transaction_.current.deferred.size(); ++i)
        this->transaction_.current.deferred[i].first(
            this->transaction_.current.deferred[i].second);
      this->transaction_.current.deferred.clear();
      if (this->transaction_.current.dirty && this->invalidateFn_)
        this->invalidateFn_(this->invalidateUserData_);
      this->transaction_.current.dirty = false;
      this->phase_ = TRACKER_PRECOMMIT;
      return STEP_DONE;
    }

    bool PushStateTracker::hasWork() const
    {
      return this->transaction_.current.hasWork() || this->transaction_.next.hasWork();
    }

    void PushStateTracker::installRoute(StateBase *state)
    {
#ifdef LOKA_LIFECYCLE_AUDIT
      assert(state->currentTracker == 0 || state->currentTracker == this);
#endif
      state->currentTracker = this;
    }

    void PushStateTracker::removeRoutes()
    {
      for (StateEntry *e = this->statesHead_; e; e = e->next)
        if (e->state->currentTracker == this)
          e->state->currentTracker = 0;
      this->phase_ = TRACKER_IDLE;
    }

    bool PushStateTracker::drainDeferred(OperationBudget &budget)
    {
      // Keep the empty intakes' capacity on ordinary successful legacy clocks.
      if (this->transaction_.current.deferred.empty() &&
          this->transaction_.next.deferred.empty())
        return true;
      // The first pass discharges leftover obligations even after refusal has
      // spent every round. Only callbacks queued again consume retry rounds.
      for (;;)
      {
        DeferredList current;
        DeferredList next;
        current.swap(this->transaction_.current.deferred);
        next.swap(this->transaction_.next.deferred);
        for (size_t i = 0; i < current.size(); ++i)
          current[i].first(current[i].second);
        for (size_t i = 0; i < next.size(); ++i)
          next[i].first(next[i].second);
        if (this->transaction_.current.deferred.empty() &&
            this->transaction_.next.deferred.empty())
          return true;
        if (budget.rounds == 0)
        {
          this->transaction_.current.deferred.clear();
          this->transaction_.next.deferred.clear();
          return false;
        }
        --budget.rounds;
      }
    }

    void PushStateTracker::releaseClockLevel()
    {
      assert(this->depth_ == 1);
      this->depth_ = 0;
    }

    bool PushStateTracker::settleCurrentTransaction(size_t &iterationsRemaining)
    {
      while (!transaction_.current.dirtyStates.empty() && iterationsRemaining > 0)
      {
        --iterationsRemaining;
        this->scratch_.swap(transaction_.current.dirtyStates);
        for (size_t i = 0; i < this->scratch_.size(); ++i)
        {
          StateBase *state = this->scratch_[i];
          if (!state)
            continue;
          bool alreadyCommitted = false;
          for (size_t committedIndex = 0;
               committedIndex < transaction_.committedDirtyStates.size();
               ++committedIndex)
          {
            if (transaction_.committedDirtyStates[committedIndex] == state)
            {
              alreadyCommitted = true;
              break;
            }
          }
          if (!alreadyCommitted)
          {
            transaction_.committedDirtyStates.push_back(state);
          }
          if (!state->recompute())
          {
            continue;
          }
          DependencyMap::iterator it = dependents.find(state);
          if (it == dependents.end())
          {
            continue;
          }
          for (size_t dependentIndex = 0;
               dependentIndex < it->second.states.size();
               ++dependentIndex)
          {
            markDirty(it->second.states[dependentIndex]);
          }
        }
        this->scratch_.clear();
      }
      return transaction_.current.dirtyStates.empty();
    }

    void PushStateTracker::TransactionIntake::clear()
    {
      dirtyStates.clear();
      deferred.clear();
      dirty = false;
    }

    bool PushStateTracker::TransactionIntake::hasWork() const
    {
      return dirty || !dirtyStates.empty() || !deferred.empty();
    }

    void PushStateTracker::TransactionIntake::removeState(StateBase *state)
    {
      for (size_t i = 0; i < dirtyStates.size();)
      {
        if (dirtyStates[i] == state)
        {
          dirtyStates.erase(dirtyStates.begin() + i);
        }
        else
        {
          ++i;
        }
      }
    }

    void PushStateTracker::TransactionIntake::swap(TransactionIntake &other)
    {
      dirtyStates.swap(other.dirtyStates);
      deferred.swap(other.deferred);
      const bool otherDirty = other.dirty;
      other.dirty = dirty;
      dirty = otherDirty;
    }

    void PushStateTracker::TrackerTransaction::begin()
    {
      current.clear();
      next.clear();
      committedDirtyStates.clear();
      committedIdentitiesErased = false;
      anyDirty = false;
    }

    PushStateTracker::TransactionIntake &
    PushStateTracker::TrackerTransaction::intake(TrackerPhase phase)
    {
      return phase == TRACKER_COMMIT ? next : current;
    }

    void PushStateTracker::TrackerTransaction::advance()
    {
      current.clear();
      current.swap(next);
      next.clear();
      committedDirtyStates.clear();
      committedIdentitiesErased = false;
    }

    void PushStateTracker::TrackerTransaction::removeState(StateBase *state)
    {
      current.removeState(state);
      next.removeState(state);
      for (size_t i = 0; i < committedDirtyStates.size();)
      {
        if (committedDirtyStates[i] == state)
        {
          committedDirtyStates.erase(committedDirtyStates.begin() + i);
          committedIdentitiesErased = true;
        }
        else
        {
          ++i;
        }
      }
    }

    PushStateTracker::StateEntry *PushStateTracker::allocateEntry(StateBase *state)
    {
      StateEntry *entry = 0;
      if (freeEntries_)
      {
        entry = freeEntries_;
        freeEntries_ = freeEntries_->next;
      }
      else
      {
        // The first row is inline; amortize further unreserved growth. The
        // chunk chain owns only heap rows, and removals recycle either kind.
        enum { kEntryChunkCapacity = 16 };
        allocateEntries(kEntryChunkCapacity);
        entry = freeEntries_;
        freeEntries_ = freeEntries_->next;
      }
      entry->state = state;
      entry->next = 0;
      return entry;
    }

    void PushStateTracker::allocateEntries(size_t count)
    {
      StateEntry *entries = new StateEntry[count];
      StateEntryChunk *chunk = new StateEntryChunk();
      chunk->entries = entries;
      chunk->count = count;
      chunk->next = chunks_;
      chunks_ = chunk;
      for (size_t i = 0; i < count; ++i)
      {
        StateEntry *entry = &entries[i];
        entry->next = freeEntries_;
        freeEntries_ = entry;
      }
    }

    void PushStateTracker::releaseEntries()
    {
      statesHead_ = 0;
      statesTail_ = 0;
      freeEntries_ = 0;
      while (chunks_)
      {
        StateEntryChunk *next = chunks_->next;
        delete[] chunks_->entries;
        delete chunks_;
        chunks_ = next;
      }
    }

    // A transaction is closed only when no level is open and no settlement is
    // running: a level joined during settlement (begin() while phase_ is not
    // idle) sees depth_ == 0 yet must not read or clear the owner's pending
    // dirt; the outer end() owns it until the phase returns to idle.
    bool PushStateTracker::peekDirty() const
    {
      return depth_ == 0 && phase_ == TRACKER_IDLE && pendingDirty_;
    }

    bool PushStateTracker::consumeDirty()
    {
      if (depth_ > 0 || phase_ != TRACKER_IDLE)
      {
        return false;
      }
      bool dirty = pendingDirty_;
      pendingDirty_ = false;
      return dirty;
    }

    void PushStateTracker::registerDependency(StateBase *dependent, StateBase *dependency)
    {
      if (!dependent || !dependency)
      {
        return;
      }
      StateList &list = dependents[dependency].states;
      for (size_t i = 0; i < list.size(); ++i)
      {
        if (list[i] == dependent)
        {
          return;
        }
      }
      list.push_back(dependent);
    }

    TrackerPhase PushStateTracker::phase() const
    {
      return phase_;
    }

  } // namespace core
} // namespace loka
