#ifndef LOKA_DSL_STATESTREAM_HPP
#define LOKA_DSL_STATESTREAM_HPP

#include <cassert>
#include <vector>

#include "loka/core/Profiler.hpp"
#include "loka/core/State.hpp"
#include "app/scene/NodeState.hpp"
#include "app/scene/StateOwner.hpp"
#include "loka/dsl/Expr.hpp"
#include "loka/dsl/Slot.hpp"

namespace loka
{
  namespace dsl
  {
    struct StateStreamBindingEntry
    {
      StateStreamBindingEntry()
          : source(0), cb(0), userData(0), destroy(0) {}
      StateStreamBindingEntry(::loka::core::StateBase *s,
                              ::loka::core::StateBase::OnChangeFn c,
                              void *u,
                              void (*d)(void *))
          : source(s), cb(c), userData(u), destroy(d) {}
      ::loka::core::StateBase *source;
      ::loka::core::StateBase::OnChangeFn cb;
      void *userData;
      void (*destroy)(void *);
    };

    template <typename T>
    class StateStream
    {
    public:
      StateStream() : state_(0), tracker_(0), owner_(0), ownsState_(false), bindings_(), slot(1) {}
      StateStream(::loka::core::State<T> *state,
                  ::loka::core::StateTracker *tracker,
                  ::loka::app::scene::IStateOwner *owner)
          : state_(state), tracker_(tracker), owner_(owner), ownsState_(false), bindings_(), slot(1) {}
      StateStream(::loka::core::State<T> *state,
                  ::loka::core::StateTracker *tracker,
                  ::loka::app::scene::IStateOwner *owner,
                  bool ownsState)
          : state_(state), tracker_(tracker), owner_(owner), ownsState_(ownsState), bindings_(), slot(1) {}
      StateStream(const StateStream &other)
          : state_(other.state_),
            tracker_(other.tracker_),
            owner_(other.owner_),
            ownsState_(other.ownsState_),
            bindings_(other.bindings_),
            slot(1)
      {
        other.state_ = 0;
        other.tracker_ = 0;
        other.owner_ = 0;
        other.ownsState_ = false;
        other.bindings_.clear();
      }
      ~StateStream()
      {
        this->releaseOwnedState();
      }

      StateStream &operator=(const StateStream &other)
      {
        if (this != &other)
        {
          this->releaseOwnedState();
          state_ = other.state_;
          tracker_ = other.tracker_;
          owner_ = other.owner_;
          ownsState_ = other.ownsState_;
          bindings_ = other.bindings_;
          other.state_ = 0;
          other.tracker_ = 0;
          other.owner_ = 0;
          other.ownsState_ = false;
          other.bindings_.clear();
        }
        return *this;
      }

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
        StateStream<typename Mapper::Result> out(derived, this->tracker_, this->owner_, true);
        this->transferBindingsTo(out);
        return out;
      }

      template <typename R, typename ExprT>
      StateStream<R> map(const Expr<R, ExprT> &expr) const
      {
        PROFILE_SECTION_ID("sMapExpr", 11);
        if (!this->state_)
        {
          return StateStream<R>(0, this->tracker_, this->owner_);
        }
        assert(this->owner_ && "StateStream::map(expr) requires IStateOwner");
        MapSlotExprEval<T, R, ExprT> *eval = new MapSlotExprEval<T, R, ExprT>(this->state_, expr);
        ::loka::core::DerivedState<R> *derived =
            new ::loka::core::DerivedState<R>(this->state_, eval);
        this->adoptDerived(derived);
        this->bindRecompute(this->state_, derived);
        StateStream<R> out(derived, this->tracker_, this->owner_, true);
        this->transferBindingsTo(out);
        return out;
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
        StateStream<typename Combiner::Result> out(derived, this->tracker_, this->owner_, true);
        this->transferBindingsTo(out);
        other.transferBindingsTo(out);
        return out;
      }

      void set(::loka::app::scene::NodeState<T> &target, bool forceUpdate = false) const
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
        this->addBinding(this->state_, &SetBinding::ApplyThunk, binding, &SetBinding::Destroy);
      }

      void releaseOwnedState()
      {
        for (size_t i = 0; i < bindings_.size(); ++i)
        {
          StateStreamBindingEntry &entry = bindings_[i];
          if (entry.source && entry.cb)
          {
            entry.source->deferUnbind(entry.cb, entry.userData);
          }
          if (entry.destroy)
          {
            entry.destroy(entry.userData);
          }
        }
        bindings_.clear();
        if (ownsState_ && owner_ && state_)
        {
          owner_->releaseState(state_);
        }
        state_ = 0;
        ownsState_ = false;
      }

    private:
      void addBinding(::loka::core::StateBase *source,
                      ::loka::core::StateBase::OnChangeFn cb,
                      void *userData,
                      void (*destroy)(void *)) const
      {
        bindings_.push_back(StateStreamBindingEntry(source, cb, userData, destroy));
      }

      template <typename U>
      void transferBindingsTo(StateStream<U> &out) const
      {
        for (size_t i = 0; i < bindings_.size(); ++i)
        {
          out.bindings_.push_back(bindings_[i]);
        }
        bindings_.clear();
      }

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

      template <typename SrcT, typename R, typename ExprT>
      struct MapSlotExprEval : public ::loka::core::DerivedState<R>::EvalFn
      {
        MapSlotExprEval(::loka::core::State<SrcT> *state, const Expr<R, ExprT> &expr)
            : state_(state), expr_(expr) {}

        R operator()()
        {
          SrcT value = state_->get();
          EvalContext ctx;
          ctx.slots[1] = &value;
          return expr_.eval(ctx);
        }

        ::loka::core::State<SrcT> *state_;
        Expr<R, ExprT> expr_;
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
                   ::loka::app::scene::NodeState<T> *target,
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

        static void Destroy(void *userData)
        {
          delete static_cast<SetBinding *>(userData);
        }

        ::loka::core::State<T> *state_;
        ::loka::app::scene::NodeState<T> *target_;
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

        static void Destroy(void *userData)
        {
          delete static_cast<RecomputeBinding *>(userData);
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
        this->addBinding(source, &RecomputeBinding::ApplyThunk, binding, &RecomputeBinding::Destroy);
      }

      mutable ::loka::core::State<T> *state_;
      mutable ::loka::core::StateTracker *tracker_;
      mutable ::loka::app::scene::IStateOwner *owner_;
      mutable bool ownsState_;
      mutable std::vector<StateStreamBindingEntry> bindings_;
      template <typename U>
      friend class StateStream;
    public:
      ValueSlot<T> slot;
    };
  } // namespace dsl
} // namespace loka

namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <typename T>
      inline loka::dsl::StateStream<T> NodeState<T>::stream() const
      {
        return loka::dsl::StateStream<T>(this->state_, this->tracker_, this->owner_);
      }
    } // namespace scene
  }   // namespace app
} // namespace loka

#endif // LOKA_DSL_STATESTREAM_HPP
