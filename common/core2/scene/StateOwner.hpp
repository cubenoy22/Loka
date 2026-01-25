#ifndef LOKA_CORE2_SCENE_STATEOWNER_HPP
#define LOKA_CORE2_SCENE_STATEOWNER_HPP

namespace loka
{
  namespace core
  {
    class StateBase;
    class StateTracker;
  }

  namespace core
  {
    namespace scene
    {
      class IStateOwner
      {
      public:
        virtual ~IStateOwner() {}
        virtual void adoptState(StateBase *state) = 0;
        virtual void adoptStateUnchecked(StateBase *state) = 0;
        virtual void reserveStates(size_t count) = 0;
        virtual void reserveStateArena(size_t totalSize) = 0;
        virtual void *allocateStateMemory(size_t size, size_t align) = 0;
        virtual void registerStateMemory(StateBase *state, void (*destroy)(StateBase *)) = 0;
        virtual StateTracker *tracker() = 0;
      };
    } // namespace scene
  } // namespace core
} // namespace loka

#endif // LOKA_CORE2_SCENE_STATEOWNER_HPP
