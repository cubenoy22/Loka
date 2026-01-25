#ifndef LOKA_CORE2_SCENE_BOUND_STATE_HPP
#define LOKA_CORE2_SCENE_BOUND_STATE_HPP

#include <cassert>
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"
#include "core2/scene/StateOwner.hpp"

namespace loka
{
  namespace dsl
  {
    template <typename T>
    class StateStream;
  }
}

namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <typename T>
      class BoundState
      {
      public:
        BoundState() : state_(0), tracker_(0), owner_(0) {}
        BoundState(loka::core::MutableState<T> *state, loka::core::StateTracker *tracker)
            : state_(state), tracker_(tracker), owner_(0) {}
        BoundState(loka::core::MutableState<T> *state, loka::core::StateTracker *tracker, IStateOwner *owner)
            : state_(state), tracker_(tracker), owner_(owner) {}

        bool isValid() const { return state_ != 0; }
        loka::core::MutableState<T> *mutableState() const { return state_; }
        loka::core::State<T> *state() const { return state_; }
        operator loka::core::State<T> *() const { return state_; }
        loka::core::StateTracker *tracker() const { return tracker_; }
        IStateOwner *owner() const { return owner_; }

        T get() const
        {
          assert(state_ && "BoundState::get requires a state");
          return state_->get();
        }

        void set(const T &value, bool forceUpdate = false)
        {
          assert(state_ && "BoundState::set requires a state");
          if (tracker_ && tracker_->phase() == loka::core::TRACKER_IDLE)
          {
            tracker_->begin();
            state_->set(value, forceUpdate);
            tracker_->end();
            return;
          }
          state_->set(value, forceUpdate);
        }

        void bind(typename loka::core::State<T>::OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0)
        {
          if (state_)
          {
            state_->bind(cb, userData, callImmediately, callOnce, priority);
          }
        }

        void unbind(typename loka::core::State<T>::OnChangeFn cb, void *userData)
        {
          if (state_)
          {
            state_->unbind(cb, userData);
          }
        }

        loka::core::MutableState<T> &unwrap() const
        {
          assert(state_ && "BoundState::unwrap requires a state");
          return *state_;
        }

        loka::dsl::StateStream<T> stream() const;

      private:
        loka::core::MutableState<T> *state_;
        loka::core::StateTracker *tracker_;
        IStateOwner *owner_;
      };

    } // namespace scene
  }   // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUND_STATE_HPP
