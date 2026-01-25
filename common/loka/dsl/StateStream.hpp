#ifndef LOKA_DSL_STATESTREAM_HPP
#define LOKA_DSL_STATESTREAM_HPP

#include <cassert>
#include <vector>

#include "core/Profiler.hpp"
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
      StateStream(::loka::core::State<T> *state,
                  ::loka::core::StateTracker *tracker,
                  ::loka::core::scene::IStateOwner *owner)
          : state_(state), tracker_(tracker), owner_(owner) {}

      template <typename Mapper>
      StateStream<typename Mapper::Result> map(const Mapper &mapper) const
      {
        PROFILE_SECTION_ID("sMap", 1);
        if (!this->state_)
        {
          return StateStream<typename Mapper::Result>(0, this->tracker_, this->owner_);
        }
        assert(this->owner_ && "StateStream::map requires IStateOwner");
        PROFILE_SECTION_ID("sMapEvalNew", 2);
        MapEval<T, typename Mapper::Result, Mapper> *eval =
            new MapEval<T, typename Mapper::Result, Mapper>(this->state_, mapper);
        PROFILE_SECTION_ID("sMapDerNew", 3);
        ::loka::core::DerivedState<typename Mapper::Result> *derived =
            new ::loka::core::DerivedState<typename Mapper::Result>(this->state_, eval);
        this->adoptDerived(derived);
        this->bindRecompute(this->state_, derived);
        return StateStream<typename Mapper::Result>(derived, this->tracker_, this->owner_);
      }

      template <typename U, typename Combiner>
      StateStream<typename Combiner::Result> combine(const StateStream<U> &other, const Combiner &combiner) const
      {
        PROFILE_SECTION_ID("sComb", 4);
        if (!this->state_ || !other.state_)
        {
          return StateStream<typename Combiner::Result>(0, this->tracker_, this->owner_);
        }
        assert(this->owner_ && this->owner_ == other.owner_ && "StateStream::combine requires same IStateOwner");
        PROFILE_SECTION_ID("sCombEvalNew", 5);
        CombineEval<T, U, typename Combiner::Result, Combiner> *eval =
            new CombineEval<T, U, typename Combiner::Result, Combiner>(this->state_, other.state_, combiner);
        PROFILE_SECTION_ID("sCombDerNew", 6);
        ::loka::core::DerivedState<typename Combiner::Result> *derived =
            new ::loka::core::DerivedState<typename Combiner::Result>(this->state_, other.state_, eval);
        this->adoptDerived(derived);
        this->bindRecompute(this->state_, derived);
        this->bindRecompute(other.state_, derived);
        return StateStream<typename Combiner::Result>(derived, this->tracker_, this->owner_);
      }

      void set(::loka::core::scene::BoundState<T> &target, bool forceUpdate = false) const
      {
        PROFILE_SECTION_ID("sSet", 7);
        if (!this->state_)
        {
          return;
        }
        PROFILE_SECTION_ID("sSetNew", 8);
        SetBinding *binding = new SetBinding(this->state_, &target, forceUpdate);
        PROFILE_SECTION_ID("sSetApply", 9);
        binding->apply();
        PROFILE_SECTION_ID("sSetBind", 10);
        this->state_->deferBind(&SetBinding::ApplyThunk, binding);
      }

    private:
      template <typename SrcT, typename R, typename Mapper>
      struct MapEval : public ::loka::core::DerivedState<R>::EvalFn
      {
        MapEval(::loka::core::State<SrcT> *state, const Mapper &mapper)
            : state_(state), mapper_(mapper) {}

        R operator()()
        {
          return mapper_(state_->get());
        }

        ::loka::core::State<SrcT> *state_;
        Mapper mapper_;
      };

      template <typename A, typename B, typename R, typename Combiner>
      struct CombineEval : public ::loka::core::DerivedState<R>::EvalFn
      {
        CombineEval(::loka::core::State<A> *left,
                    ::loka::core::State<B> *right,
                    const Combiner &combiner)
            : left_(left), right_(right), combiner_(combiner) {}

        R operator()()
        {
          return combiner_(left_->get(), right_->get());
        }

        ::loka::core::State<A> *left_;
        ::loka::core::State<B> *right_;
        Combiner combiner_;
      };

      struct SetBinding
      {
        SetBinding(::loka::core::State<T> *state,
                   ::loka::core::scene::BoundState<T> *target,
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

        ::loka::core::State<T> *state_;
        ::loka::core::scene::BoundState<T> *target_;
        bool forceUpdate_;
      };

      void adoptDerived(::loka::core::StateBase *state) const
      {
        if (this->owner_)
        {
          this->owner_->adoptStateUnchecked(state);
          return;
        }
        if (this->tracker_)
        {
          ::loka::core::PushStateTracker *push = this->tracker_->asPushTracker();
          if (push)
          {
            push->addState(state);
          }
        }
      }

      struct RecomputeBinding
      {
        explicit RecomputeBinding(::loka::core::StateBase *state) : state_(state) {}

        static void ApplyThunk(void *userData)
        {
          RecomputeBinding *self = static_cast<RecomputeBinding *>(userData);
          if (self && self->state_)
          {
            self->state_->recompute();
          }
        }

        ::loka::core::StateBase *state_;
      };

      void bindRecompute(::loka::core::StateBase *source,
                         ::loka::core::StateBase *derived) const
      {
        if (!source || !derived)
        {
          return;
        }
        RecomputeBinding *binding = new RecomputeBinding(derived);
        source->deferBind(&RecomputeBinding::ApplyThunk, binding);
      }

      ::loka::core::State<T> *state_;
      ::loka::core::StateTracker *tracker_;
      ::loka::core::scene::IStateOwner *owner_;
    };
  } // namespace dsl
} // namespace loka

namespace loka
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
} // namespace loka

#endif // LOKA_DSL_STATESTREAM_HPP
