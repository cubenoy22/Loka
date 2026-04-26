#ifndef LOKA_CORE2_SCENE_NODE_STATE_HPP
#define LOKA_CORE2_SCENE_NODE_STATE_HPP

#include <cassert>
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"
#include "app/scene/StateOwner.hpp"

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
      class BorrowedState
      {
      public:
        BorrowedState() : state_(0) {}
        explicit BorrowedState(loka::core::State<T> *state) : state_(state) {}

        bool isValid() const { return state_ != 0; }

        T get() const
        {
          assert(state_ && "BorrowedState::get requires a state");
          return state_->get();
        }

        void bind(typename loka::core::State<T>::OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0) const
        {
          if (state_)
          {
            state_->bind(cb, userData, callImmediately, callOnce, priority);
          }
        }

        void unbind(typename loka::core::State<T>::OnChangeFn cb, void *userData) const
        {
          if (state_)
          {
            state_->unbind(cb, userData);
          }
        }

      private:
        loka::core::State<T> *state_;
      };

      template <typename T>
      class NodeState
      {
      public:
        NodeState() : state_(0), tracker_(0), owner_(0) {}
        NodeState(loka::core::MutableState<T> *state, loka::core::StateTracker *tracker)
            : state_(state), tracker_(tracker), owner_(0) {}
        NodeState(loka::core::MutableState<T> *state, loka::core::StateTracker *tracker, IStateOwner *owner)
            : state_(state), tracker_(tracker), owner_(owner) {}

        bool isValid() const { return state_ != 0; }
        loka::core::State<T> *state() const { return state_; }
        loka::core::StateTracker *dangerouslyTracker() const { return tracker_; }
        IStateOwner *dangerouslyOwner() const { return owner_; }
        loka::core::MutableState<T> *dangerouslyMutableState() const { return state_; }

        T get() const
        {
          assert(state_ && "NodeState::get requires a state");
          return state_->get();
        }

        void set(const T &value, bool forceUpdate = false)
        {
          assert(state_ && "NodeState::set requires a state");
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

        loka::core::MutableState<T> &dangerouslyUnwrapMutableState() const
        {
          assert(state_ && "NodeState::dangerouslyUnwrapMutableState requires a state");
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

#endif // LOKA_CORE2_SCENE_NODE_STATE_HPP
