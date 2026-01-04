#ifndef LOKA_CORE2_SCENE_BOUND_STATE_HPP
#define LOKA_CORE2_SCENE_BOUND_STATE_HPP

#include <cassert>
#include "core/State.hpp"
#include "core/StateTracker.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      template <typename T>
      class BoundState
      {
      public:
        BoundState() : state_(0), tracker_(0) {}
        BoundState(MutableState<T> *state, core::StateTracker *tracker)
            : state_(state), tracker_(tracker) {}

        bool isValid() const { return state_ != 0; }
        MutableState<T> *mutableState() const { return state_; }
        State<T> *state() const { return state_; }
        operator State<T> *() const { return state_; }

        T get() const
        {
          assert(state_ && "BoundState::get requires a state");
          return state_->get();
        }

        void set(const T &value, bool forceUpdate = false)
        {
          assert(state_ && "BoundState::set requires a state");
          if (tracker_ && tracker_->phase() == core::TRACKER_IDLE)
          {
            tracker_->begin();
            state_->set(value, forceUpdate);
            tracker_->end();
            return;
          }
          state_->set(value, forceUpdate);
        }

        void bind(typename State<T>::OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0)
        {
          if (state_)
          {
            state_->bind(cb, userData, callImmediately, callOnce, priority);
          }
        }

        void unbind(typename State<T>::OnChangeFn cb, void *userData)
        {
          if (state_)
          {
            state_->unbind(cb, userData);
          }
        }

        MutableState<T> &unwrap() const
        {
          assert(state_ && "BoundState::unwrap requires a state");
          return *state_;
        }

      private:
        MutableState<T> *state_;
        core::StateTracker *tracker_;
      };

    } // namespace scene
  }   // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_BOUND_STATE_HPP
