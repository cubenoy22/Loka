#ifndef LOKA_CORE2_SCENE_BOUNDARY_INNER_STATE_OWNER_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_INNER_STATE_OWNER_HPP

#include <cassert>
#include <vector>
#include "app/scene/state/StateOwner.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Lightweight state owner for scopes that live inside a Boundary.
          Boundary remains the broad owner for ordinary component state; this
          owner is for explicit subtree-local lifecycle scopes. It is
          intentionally abstract until a concrete production consumer with a
          wired failure route exists (#145); production promotion must design
          that route and its failure atomicity. */
      class BoundaryInnerStateOwner : public IStateOwner
      {
      public:
        BoundaryInnerStateOwner()
            : tracker_(),
              ownedStates_()
        {
        }
        virtual ~BoundaryInnerStateOwner()
        {
          this->clearOwnedStates();
        }

        void setInvalidateCallback(loka::core::PushStateTracker::InvalidateFn fn, void *userData)
        {
          this->tracker_.setInvalidateCallback(fn, userData);
        }

        virtual loka::core::StateTracker *tracker()
        {
          return &this->tracker_;
        }

        virtual void adoptState(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          this->ownedStates_.push_back(state);
          this->tracker_.addState(state);
        }

        virtual void adoptStateUnchecked(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          this->ownedStates_.push_back(state);
          this->tracker_.addStateUnchecked(state);
        }

        virtual void releaseState(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          for (size_t i = 0; i < this->ownedStates_.size();)
          {
            if (this->ownedStates_[i] == state)
            {
              this->ownedStates_.erase(this->ownedStates_.begin() + i);
            }
            else
            {
              ++i;
            }
          }
          this->tracker_.removeState(state);
          assert(!state->isArenaAllocated() && "BoundaryInnerStateOwner does not own arena-allocated state");
          if (!state->isArenaAllocated())
          {
            DestroyAdoptedHeapState(state);
          }
        }

        virtual void reserveStates(size_t count)
        {
          this->ownedStates_.reserve(this->ownedStates_.size() + count);
          this->tracker_.reserveStates(count);
        }

        /** An arena reservation refusal is storage-strategy degradation,
            not a logical materialization failure. Only a refusal to
            materialize at BOTH doors — the arena and the final heap door —
            becomes a compose failure (#132 ruling 3). */
        virtual void reserveStateArena(size_t totalSize)
        {
          (void)totalSize;
        }

        virtual void *allocateStateMemory(size_t size, size_t align)
        {
          (void)size;
          (void)align;
          return 0;
        }

        virtual void registerStateMemory(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
        {
          (void)state;
          (void)destroy;
        }

        void clearOwnedStates()
        {
          for (size_t i = 0; i < this->ownedStates_.size(); ++i)
          {
            loka::core::StateBase *state = this->ownedStates_[i];
            if (state)
            {
              this->tracker_.removeState(state);
              assert(!state->isArenaAllocated() && "BoundaryInnerStateOwner does not own arena-allocated state");
              if (!state->isArenaAllocated())
              {
                DestroyAdoptedHeapState(state);
              }
            }
          }
          this->ownedStates_.clear();
        }

      private:
        BoundaryInnerStateOwner(const BoundaryInnerStateOwner &);
        BoundaryInnerStateOwner &operator=(const BoundaryInnerStateOwner &);

        loka::core::PushStateTracker tracker_;
        std::vector<loka::core::StateBase *> ownedStates_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_INNER_STATE_OWNER_HPP
