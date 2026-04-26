#ifndef LOKA_CORE2_SCENE_STATEOWNER_HPP
#define LOKA_CORE2_SCENE_STATEOWNER_HPP

namespace loka
{
  namespace core
  {
    class StateBase;
    class StateTracker;
  }

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
        virtual void reserveStateArena(size_t totalSize) = 0;
        virtual void *allocateStateMemory(size_t size, size_t align) = 0;
        virtual void registerStateMemory(core::StateBase *state, void (*destroy)(core::StateBase *)) = 0;
        virtual core::StateTracker *tracker() = 0;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_STATEOWNER_HPP
