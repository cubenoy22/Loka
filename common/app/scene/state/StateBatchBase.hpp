#ifndef LOKA_CORE2_SCENE_STATE_STATEBATCHBASE_HPP
#define LOKA_CORE2_SCENE_STATE_STATEBATCHBASE_HPP

#include <cassert>
#include <new>
#include "app/scene/detail/ArenaMath.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/scene/state/StateOwner.hpp"
#include "core/LokaAlloc.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class StateBatchBase
      {
      public:
        enum
        {
          kStorageBytes = 32
        };

        struct Storage
        {
          double d;
          void *p;
          char bytes[kStorageBytes];
        };

        template <typename T> static void CopyInitial(char *storage, const T &value)
        {
          new (storage) T(value);
        }

        template <typename T> static void DestroyInitialObject(void *initialPtr)
        {
          T *initial = reinterpret_cast<T *>(initialPtr);
          initial->~T();
        }

        template <typename T> static void DestroyState(loka::core::StateBase *state)
        {
          loka::core::MutableState<T> *typed = static_cast<loka::core::MutableState<T> *>(state);
          if (typed)
          {
            typedef loka::core::MutableState<T> MutableStateType;
            typed->~MutableStateType();
          }
        }

        /** Worst-case owner arena bytes one CreateStateFromInitial call can
            consume for T: the MutableState object plus the alignment padding
            the arena may insert. Padding is bounded by the arena's effective
            alignment (NormalizeArenaAlign), not raw AlignOf, which can sit
            below the arena minimum on some ABIs. Batch reserve estimates must
            use this helper (not a hand-written copy of the arithmetic) so a
            batch reservation covers the state creations that immediately
            follow it. */
        template <typename T> static size_t ArenaBytesForState()
        {
          return sizeof(loka::core::MutableState<T>) +
                 detail::NormalizeArenaAlign(detail::AlignOf<loka::core::MutableState<T> >::value);
        }

        /** Creates from capacity already reserved by the caller, falling back
            to heap ownership when no arena capacity remains. */
        template <typename T>
        static void CreateStateFromInitial(IStateOwner *owner, NodeState<T> &out, const T &initial)
        {
          loka::core::MutableState<T> *state = 0;
          size_t align = detail::AlignOf<loka::core::MutableState<T> >::value;
          void *mem = owner ? owner->allocateStateMemory(sizeof(loka::core::MutableState<T>), align) : 0;
          if (mem)
          {
            state = new (mem) loka::core::MutableState<T>(initial);
            state->setArenaAllocated(true);
            owner->registerStateMemory(state, &DestroyState<T>);
          }
          else
          {
            // Heap fallback rides the allocation gate: nothrow, tagged, and
            // released by DestroyAdoptedHeapState under the same site so the
            // ledger balances.
            state = loka::core::LokaNew<loka::core::MutableState<T> >(HeapStateAllocationSite(), initial);
            if (state)
            {
              assert(static_cast<void *>(static_cast<loka::core::StateBase *>(state)) ==
                         static_cast<void *>(state) &&
                     "gate frees through StateBase; its subobject must sit at the storage address");
              state->setGateAllocated(true);
            }
            else if (owner)
            {
              // A 0 from the gate means the backend already gave up (#132
              // ruling 3). Both storage doors refused, so raise the owner's
              // white flag: the owning boundary converts it into a compose
              // failure at compose completion instead of adopting a silent
              // dead state. The out-parameter below still degrades to an
              // invalid NodeState (adoption is null-safe), matching the
              // null-owner degenerate shape.
              owner->noteStateAllocationFailure();
            }
          }
          if (owner)
          {
            owner->adoptStateUnchecked(state);
            out = NodeState<T>(state, owner->tracker(), owner);
          }
          else
          {
            out = NodeState<T>(state, 0, 0);
          }
        }

        /** Reserves one owner-lifetime arena slot for an immediate declaration
            before using the shared creation mechanism. */
        template <typename T> static void CreateImmediateState(IStateOwner *owner, NodeState<T> &out, const T &initial)
        {
          if (owner)
          {
            // An arena reservation refusal is storage-strategy degradation,
            // not a logical materialization failure. Only a refusal to
            // materialize at BOTH doors — the arena and the final heap door —
            // becomes a compose failure (#132 ruling 3).
            owner->reserveStateArena(ArenaBytesForState<T>());
          }
          CreateStateFromInitial<T>(owner, out, initial);
        }
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_STATE_STATEBATCHBASE_HPP
