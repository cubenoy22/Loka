#ifndef LOKA_CORE2_SCENE_STATE_STATEOWNER_HPP
#define LOKA_CORE2_SCENE_STATE_STATEOWNER_HPP

#include <cassert>
#include <new>
#include "app/scene/detail/ArenaMath.hpp"
#include "core/LokaAlloc.hpp"
#include "core/Held.hpp"
#include "core/State.hpp"

namespace loka
{
  namespace core
  {
    class StateTracker;
  } // namespace core

  namespace app
  {
    namespace scene
    {
      class BoundaryNode;

      class IStateOwner
      {
      public:
        virtual ~IStateOwner() {}
        virtual void adoptState(core::StateBase *state) = 0;
        virtual void adoptStateUnchecked(core::StateBase *state) = 0;
        virtual void releaseState(core::StateBase *state) = 0;
        virtual void reserveStates(size_t count) = 0;
        /** Ensures arena capacity for one allocation batch. An arena
            reservation refusal is storage-strategy degradation, not a logical
            materialization failure. Only a refusal to materialize at BOTH
            doors — the arena and the final heap door — becomes a compose
            failure (#132 ruling 3). */
        virtual void reserveStateArena(size_t totalSize) = 0;
        /** Allocation white flag (#132 ruling 3): the state creation path
            calls this when both storage doors — arena and gate-routed heap
            fallback — refused, so no state was materialized. This is pure
            virtual so omission cannot silently discard the failure; every
            owner must state its failure policy explicitly. */
        virtual void noteStateAllocationFailure() = 0;
        virtual void *allocateStateMemory(size_t size, size_t align) = 0;
        virtual void registerStateMemory(core::StateBase *state, void (*destroy)(core::StateBase *)) = 0;
        /** Owner-local ledger and the creating owner's resident-storage door
            for passive Held payload blocks. */
        virtual core::HoldLedger *holdLedger() = 0;
        virtual void reserveHeldArena(size_t totalSize) = 0;
        virtual void *allocateHeldMemory(size_t size, size_t align) = 0;
        virtual void registerHeldMemory(core::detail::HeldBlockBase *block) = 0;
        /** Queues a now-unowned block on this owner's existing retire pool.
            A Boundary-inner owner delegates this door to its enclosing
            Boundary; the releaser never runs at the drop site. */
        virtual void retireHeldBlock(core::detail::HeldBlockBase *block) = 0;
        virtual core::StateTracker *tracker() = 0;
        /** Connects an owner nested inside a Boundary to the arena and
            compose-failure route that enclose it. Broad owners and test
            doubles have no attachment work. */
        virtual void attachEnclosingBoundary(BoundaryNode *boundary)
        {
          (void)boundary;
        }

        void attachEnclosingHoldOwner(IStateOwner *owner)
        {
          core::HoldLedger *ledger = this->holdLedger();
          assert(ledger && "IStateOwner requires a HoldLedger");
          if (ledger)
          {
            ledger->attachEnclosingOwner(owner);
          }
        }

        core::detail::HoldAttempt tryHoldBlock(
            core::detail::HeldBlockBase *block)
        {
          core::HoldLedger *ledger = this->holdLedger();
          return ledger ? ledger->tryHold(block)
                        : core::detail::HOLD_ATTEMPT_INVALID;
        }

        void detachHeldResources()
        {
          core::HoldLedger *ledger = this->holdLedger();
          assert(ledger && "IStateOwner requires a HoldLedger");
          if (ledger)
          {
            ledger->dropAll();
          }
        }
      };

      /** Static gate site for heap-fallback states adopted by an
          IStateOwner. The allocation side (StateBatchBase) and every
          release side must use this one site so the audit ledger balances. */
      inline const loka::core::LokaAllocationSite &HeapStateAllocationSite()
      {
        static const loka::core::LokaAllocationSite site("StateOwner", "MutableState");
        return site;
      }

      /** Destroys one adopted non-arena state through the door its storage
          came from: gate-resident states return to the backend under
          HeapStateAllocationSite(), plain-new states (dangerouslyUse*,
          test fixtures) keep operator delete. Null-safe. */
      inline void DestroyAdoptedHeapState(loka::core::StateBase *state)
      {
        if (!state)
        {
          return;
        }
        if (state->isGateAllocated())
        {
          // The virtual destructor destroys the most-derived state; the
          // creation path asserts the StateBase subobject sits at the
          // storage address LokaAllocRaw returned.
          state->~StateBase();
          loka::core::LokaFreeRaw(state, HeapStateAllocationSite());
          return;
        }
        delete state;
      }
    } // namespace scene
  } // namespace app

  namespace core
  {
    inline HoldLedger::HoldLedger(loka::app::scene::IStateOwner *owner)
        : owner_(owner),
          head_(0)
#ifdef LOKA_LIFECYCLE_AUDIT
          , enclosingOwner_(0)
#endif
    {
      assert(this->owner_ && "HoldLedger requires an owner scope");
    }

    inline HoldLedger::~HoldLedger()
    {
      this->auditEmptyBeforeReclaim();
    }

    inline bool HoldLedger::empty() const
    {
      return this->head_ == 0;
    }

    inline void HoldLedger::auditEmptyBeforeReclaim() const
    {
#ifdef LOKA_LIFECYCLE_AUDIT
      assert(this->empty() &&
             "owner reclaimed before its Held owner slots detached");
#endif
    }

    inline void HoldLedger::attachEnclosingOwner(
        loka::app::scene::IStateOwner *owner)
    {
#ifdef LOKA_LIFECYCLE_AUDIT
      if (!owner || owner == this->owner_)
      {
        return;
      }
      assert((!this->enclosingOwner_ || this->enclosingOwner_ == owner) &&
             "an owner scope cannot move between Held ownership subtrees");
      this->enclosingOwner_ = owner;
#else
      (void)owner;
#endif
    }

#ifdef LOKA_LIFECYCLE_AUDIT
    inline bool HoldLedger::ownerIsInsideCreatorSubtree(
        loka::app::scene::IStateOwner *creator) const
    {
      loka::app::scene::IStateOwner *cursor = this->owner_;
      while (cursor)
      {
        if (cursor == creator)
        {
          return true;
        }
        HoldLedger *ledger = cursor->holdLedger();
        cursor = ledger ? ledger->enclosingOwner_ : 0;
      }
      return false;
    }
#endif

    inline detail::HoldAttempt HoldLedger::tryHold(
        detail::HeldBlockBase *block)
    {
      if (!block || block->releaseQueued_ || block->released_)
      {
        return detail::HOLD_ATTEMPT_INVALID;
      }

      detail::HoldSlot *emptySlot = 0;
      for (unsigned i = 0; i < detail::HeldBlockBase::kSlotCapacity; ++i)
      {
        detail::HoldSlot &slot = block->slots_[i];
        if (slot.ownerKey == this->owner_)
        {
          assert(slot.count != static_cast<unsigned>(-1) &&
                 "Held owner slot count overflow");
          ++slot.count;
          return detail::HOLD_ATTEMPT_ACCEPTED;
        }
        if (!slot.ownerKey && !emptySlot)
        {
          emptySlot = &slot;
        }
      }

      if (!emptySlot)
      {
        return detail::HOLD_ATTEMPT_SLOT_OVERFLOW;
      }
#ifdef LOKA_LIFECYCLE_AUDIT
      if (!this->ownerIsInsideCreatorSubtree(block->creator_))
      {
        return detail::HOLD_ATTEMPT_OUTSIDE_CREATOR_SUBTREE;
      }
#endif

      emptySlot->ownerKey = this->owner_;
      emptySlot->count = 1;
      emptySlot->previous = 0;
      emptySlot->next = this->head_;
      if (this->head_)
      {
        this->head_->previous = emptySlot;
      }
      this->head_ = emptySlot;
      ++block->activeSlots_;
      return detail::HOLD_ATTEMPT_ACCEPTED;
    }

    inline void HoldLedger::dropAll()
    {
      while (this->head_)
      {
        detail::HoldSlot *slot = this->head_;
        detail::HeldBlockBase *block = slot->block;
        this->head_ = slot->next;
        if (this->head_)
        {
          this->head_->previous = 0;
        }
        slot->ownerKey = 0;
        slot->count = 0;
        slot->previous = 0;
        slot->next = 0;
        assert(block && block->activeSlots_ > 0 &&
               "HoldLedger slot requires a live block row");
        --block->activeSlots_;
        if (block->activeSlots_ == 0)
        {
          block->markReleaseQueued();
          this->owner_->retireHeldBlock(block);
        }
      }
    }

    template <typename T>
    Held<T> Held<T>::Create(loka::app::scene::IStateOwner *owner,
                            T *value,
                            ReleaserFn releaser)
    {
      if (!owner || !value || !releaser)
      {
        return Held<T>();
      }
      typedef detail::HeldBlock<T> Block;
      const size_t align = loka::app::scene::detail::AlignOf<Block>::value;
      owner->reserveHeldArena(
          sizeof(Block) + loka::app::scene::detail::NormalizeArenaAlign(align));
      void *storage = owner->allocateHeldMemory(sizeof(Block), align);
      if (!storage)
      {
        return Held<T>();
      }
      Block *block = new (storage) Block(owner, value, releaser);
      const detail::HoldAttempt result = owner->tryHoldBlock(block);
      assert(result == detail::HOLD_ATTEMPT_ACCEPTED &&
             "a new Held block must start in its creating owner's slot");
      if (result != detail::HOLD_ATTEMPT_ACCEPTED)
      {
        block->~Block();
        return Held<T>();
      }
      owner->registerHeldMemory(block);
      return Held<T>(block);
    }
  } // namespace core
} // namespace loka

#endif // LOKA_CORE2_SCENE_STATE_STATEOWNER_HPP
