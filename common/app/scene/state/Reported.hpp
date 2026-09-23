#ifndef LOKA_APP_SCENE_STATE_REPORTED_HPP
#define LOKA_APP_SCENE_STATE_REPORTED_HPP
#include "app/scene/state/NodeState.hpp"
namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <class PropsT> struct NodePropsBase;
      class ComposableNode;
      class StateBatchBase;
      /** Owner-declared fact. The app reads; the reporting seam publishes.
          Copies borrow the same owner storage and do not extend its lifetime.
          Core State assignment/asMutableState remain explicit escape hatches. */
      template <typename T> class Reported
      {
      public:
        Reported()
            : seat_()
        {
        }
        bool isValid() const
        {
          return this->seat_.isValid();
        }
        core::State<T> *state() const
        {
          return this->seat_.state();
        }

      private:
        friend class ComposableNode;
        friend class StateBatchBase;
        template <class PropsT> friend struct NodePropsBase;
        NodeState<T> seat_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
