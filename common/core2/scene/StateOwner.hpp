#ifndef LOKA_CORE2_SCENE_STATEOWNER_HPP
#define LOKA_CORE2_SCENE_STATEOWNER_HPP

namespace declara
{
  namespace core
  {
    class StateBase;
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
      };
    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_STATEOWNER_HPP
