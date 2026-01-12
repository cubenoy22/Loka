#ifndef LOKA_DSL_STATESTREAM_HPP
#define LOKA_DSL_STATESTREAM_HPP

#include <cassert>
#include <vector>

#include "core/State.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/StateOwner.hpp"

namespace loka
{
  namespace dsl
  {
    template <typename T>
    class StateStream
    {
    public:
      StateStream() : state_(0), tracker_(0), owner_(0) {}
      StateStream(::declara::core::State<T> *state,
                  ::declara::core::StateTracker *tracker,
                  ::declara::core::scene::IStateOwner *owner)
          : state_(state), tracker_(tracker), owner_(owner) {}

      template <typename Mapper>
      StateStream<typename Mapper::Result> map(const Mapper &mapper) const
      {
        if (!this->state_)
        {
          return StateStream<typename Mapper::Result>(0, this->tracker_, this->owner_);
        }
        assert(this->owner_ && "StateStream::map requires IStateOwner");
        MapEval<T, typename Mapper::Result, Mapper> *eval =
            new MapEval<T, typename Mapper::Result, Mapper>(this->state_, mapper);
        ::declara::core::DerivedState<typename Mapper::Result> *derived =
            new ::declara::core::DerivedState<typename Mapper::Result>(this->state_, eval);
        this->adoptDerived(derived);
        this->bindRecompute(this->state_, derived);
        return StateStream<typename Mapper::Result>(derived, this->tracker_, this->owner_);
      }

      template <typename U, typename Combiner>
      StateStream<typename Combiner::Result> combine(const StateStream<U> &other, const Combiner &combiner) const
      {
        if (!this->state_ || !other.state_)
        {
          return StateStream<typename Combiner::Result>(0, this->tracker_, this->owner_);
        }
        assert(this->owner_ && this->owner_ == other.owner_ && "StateStream::combine requires same IStateOwner");
        std::vector< ::declara::core::StateBase *> deps;
        deps.push_back(this->state_);
        deps.push_back(other.state_);
        CombineEval<T, U, typename Combiner::Result, Combiner> *eval =
            new CombineEval<T, U, typename Combiner::Result, Combiner>(this->state_, other.state_, combiner);
        ::declara::core::DerivedState<typename Combiner::Result> *derived =
            new ::declara::core::DerivedState<typename Combiner::Result>(deps, eval);
        this->adoptDerived(derived);
        this->bindRecompute(this->state_, derived);
        this->bindRecompute(other.state_, derived);
        return StateStream<typename Combiner::Result>(derived, this->tracker_, this->owner_);
      }

      void set(::declara::core::scene::BoundState<T> &target, bool forceUpdate = false) const
      {
        if (!this->state_)
        {
          return;
        }
        SetBinding *binding = new SetBinding(this->state_, &target, forceUpdate);
        binding->apply();
        this->state_->deferBind(&SetBinding::ApplyThunk, binding);
      }

    private:
      template <typename SrcT, typename R, typename Mapper>
      struct MapEval : public ::declara::core::DerivedState<R>::EvalFn
      {
        MapEval(::declara::core::State<SrcT> *state, const Mapper &mapper)
            : state_(state), mapper_(mapper) {}

        R operator()()
        {
          return mapper_(state_->get());
        }

        ::declara::core::State<SrcT> *state_;
        Mapper mapper_;
      };

      template <typename A, typename B, typename R, typename Combiner>
      struct CombineEval : public ::declara::core::DerivedState<R>::EvalFn
      {
        CombineEval(::declara::core::State<A> *left,
                    ::declara::core::State<B> *right,
                    const Combiner &combiner)
            : left_(left), right_(right), combiner_(combiner) {}

        R operator()()
        {
          return combiner_(left_->get(), right_->get());
        }

        ::declara::core::State<A> *left_;
        ::declara::core::State<B> *right_;
        Combiner combiner_;
      };

      struct SetBinding
      {
        SetBinding(::declara::core::State<T> *state,
                   ::declara::core::scene::BoundState<T> *target,
                   bool forceUpdate)
            : state_(state), target_(target), forceUpdate_(forceUpdate) {}

        static void ApplyThunk(void *userData)
        {
          SetBinding *self = static_cast<SetBinding *>(userData);
          if (self)
          {
            self->apply();
          }
        }

        void apply()
        {
          if (!state_ || !target_)
          {
            return;
          }
          target_->set(state_->get(), forceUpdate_);
        }

        ::declara::core::State<T> *state_;
        ::declara::core::scene::BoundState<T> *target_;
        bool forceUpdate_;
      };

      void adoptDerived(::declara::core::StateBase *state) const
      {
        if (this->owner_)
        {
          this->owner_->adoptStateUnchecked(state);
          return;
        }
        if (this->tracker_)
        {
          ::declara::core::PushStateTracker *push = this->tracker_->asPushTracker();
          if (push)
          {
            push->addState(state);
          }
        }
      }

      struct RecomputeBinding
      {
        explicit RecomputeBinding(::declara::core::StateBase *state) : state_(state) {}

        static void ApplyThunk(void *userData)
        {
          RecomputeBinding *self = static_cast<RecomputeBinding *>(userData);
          if (self && self->state_)
          {
            self->state_->recompute();
          }
        }

        ::declara::core::StateBase *state_;
      };

      void bindRecompute(::declara::core::StateBase *source,
                         ::declara::core::StateBase *derived) const
      {
        if (!source || !derived)
        {
          return;
        }
        RecomputeBinding *binding = new RecomputeBinding(derived);
        source->deferBind(&RecomputeBinding::ApplyThunk, binding);
      }

      ::declara::core::State<T> *state_;
      ::declara::core::StateTracker *tracker_;
      ::declara::core::scene::IStateOwner *owner_;
    };
  } // namespace dsl
} // namespace loka

namespace declara
{
  namespace core
  {
    namespace scene
    {
      template <typename T>
      inline loka::dsl::StateStream<T> BoundState<T>::stream() const
      {
        return loka::dsl::StateStream<T>(this->state_, this->tracker_, this->owner_);
      }
    } // namespace scene
  }   // namespace core
} // namespace declara

#endif // LOKA_DSL_STATESTREAM_HPP
