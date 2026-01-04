#ifndef LOKA_CORE2_SCENE_STATEOWNER_HPP
#define LOKA_CORE2_SCENE_STATEOWNER_HPP

namespace declara
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
        virtual void adoptState(core::StateBase *state) = 0;
        virtual core::StateTracker *tracker() = 0;
      };
    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_STATEOWNER_HPP
