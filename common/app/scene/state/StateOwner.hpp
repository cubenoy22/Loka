#ifndef LOKA_CORE2_SCENE_STATE_STATEOWNER_HPP
#define LOKA_CORE2_SCENE_STATE_STATEOWNER_HPP

#include "core/LokaAlloc.hpp"
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
      class IStateOwner
      {
      public:
        virtual ~IStateOwner() {}
        virtual void adoptState(core::StateBase *state) = 0;
        virtual void adoptStateUnchecked(core::StateBase *state) = 0;
        virtual void releaseState(core::StateBase *state) = 0;
        virtual void reserveStates(size_t count) = 0;
        /** Ensures arena capacity for one allocation batch. Returns false
            only when growth was needed and the backend refused it (#132
            ruling 3); creation below is still legal — it takes the heap
            door, and a refusal there raises the white flag itself. */
        virtual bool reserveStateArena(size_t totalSize) = 0;
        /** Allocation white flag (#132 ruling 3): the state creation path
            calls this when both storage doors — arena and gate-routed heap
            fallback — refused, so no state was materialized. The default
            ignores the flag; boundary owners convert it into a compose
            failure at compose completion. */
        virtual void noteStateAllocationFailure() {}
        virtual void *allocateStateMemory(size_t size, size_t align) = 0;
        virtual void registerStateMemory(core::StateBase *state, void (*destroy)(core::StateBase *)) = 0;
        virtual core::StateTracker *tracker() = 0;
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
} // namespace loka

#endif // LOKA_CORE2_SCENE_STATE_STATEOWNER_HPP
