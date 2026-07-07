#ifndef LOKA_CORE2_SCENE_STATE_STATEBATCHBASE_HPP
#define LOKA_CORE2_SCENE_STATE_STATEBATCHBASE_HPP

#include <new>
#include "app/scene/detail/ArenaMath.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/scene/state/StateOwner.hpp"

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
            one-shot reserve is never smaller than the creates that follow. */
        template <typename T> static size_t ArenaBytesForState()
        {
          return sizeof(loka::core::MutableState<T>) +
                 detail::NormalizeArenaAlign(detail::AlignOf<loka::core::MutableState<T> >::value);
        }

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
            state = new loka::core::MutableState<T>(initial);
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

        template <typename T> static void CreateImmediateState(IStateOwner *owner, NodeState<T> &out, const T &initial)
        {
          CreateStateFromInitial<T>(owner, out, initial);
        }
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_STATE_STATEBATCHBASE_HPP
