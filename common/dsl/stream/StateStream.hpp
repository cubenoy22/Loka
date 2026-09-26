#ifndef LOKA_DSL_STATESTREAM_HPP
#define LOKA_DSL_STATESTREAM_HPP

#include <cassert>
#include <vector>

#include "core/Profiler.hpp"
#include "core/State.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/scene/state/StateOwner.hpp"
#include "dsl/flow/Expr.hpp"
#include "dsl/stream/Slot.hpp"

namespace loka
{
  namespace dsl
  {
    struct StateStreamBindingEntry
    {
      StateStreamBindingEntry()
          : source(0),
            cb(0),
            userData(0),
            destroy(0),
            ownedState(0)
      {
      }
      StateStreamBindingEntry(::loka::core::StateBase *s,
                              ::loka::core::StateBase::OnChangeFn c,
                              void *u,
                              void (*d)(void *))
          : source(s),
            cb(c),
            userData(u),
            destroy(d),
            ownedState(0)
      {
      }
      ::loka::core::StateBase *source;
      ::loka::core::StateBase::OnChangeFn cb;
      void *userData;
      void (*destroy)(void *);
      /** State released after every subscription in this batch is disconnected. */
      ::loka::core::StateBase *ownedState;
    };

    template <typename T> class StateStream
    {
    public:
      StateStream()
          : state_(0),
            tracker_(0),
            owner_(0),
            ownsState_(false),
            bindings_(),
            slot(1)
      {
      }
      StateStream(::loka::core::State<T> *state,
                  ::loka::core::StateTracker *tracker,
                  ::loka::app::scene::IStateOwner *owner)
          : state_(state),
            tracker_(tracker),
            owner_(owner),
            ownsState_(false),
            bindings_(),
            slot(1)
      {
      }
      StateStream(::loka::core::State<T> *state,
                  ::loka::core::StateTracker *tracker,
                  ::loka::app::scene::IStateOwner *owner,
                  bool ownsState)
          : state_(state),
            tracker_(tracker),
            owner_(owner),
            ownsState_(ownsState),
            bindings_(),
            slot(1)
      {
      }
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

      /**
       * Consume an owning input, including a named stream, into a linear chain.
       * The final stream (or its FlowSlot) owns every intermediate State;
       * fan-out from one owning intermediate is not supported. Borrowed roots
       * remain reusable, as they do not transfer State ownership.
       */
      template <typename Mapper> StateStream<typename Mapper::Result> map(const Mapper &mapper) const
      {
        PROFILE_SECTION("sMap");
        if (!this->state_)
        {
          return StateStream<typename Mapper::Result>(0, this->tracker_, this->owner_);
        }
        assert(this->owner_ && "StateStream::map requires IStateOwner");
        PROFILE_SECTION("sMapEvalNew");
        MapEval<T, typename Mapper::Result, Mapper> *eval =
            new MapEval<T, typename Mapper::Result, Mapper>(this->state_, mapper);
        PROFILE_SECTION("sMapDerNew");
        // Explicit recompute bindings are the only update evaluator; empty
        // dependencies prevent a second tracker route. Adoption still owns the State.
        ::loka::core::DerivedState<typename Mapper::Result> *derived =
            new ::loka::core::DerivedState<typename Mapper::Result>(std::vector< ::loka::core::StateBase *>(), eval);
        this->adoptDerived(derived);
        this->bindRecompute(this->state_, derived);
        StateStream<typename Mapper::Result> out(derived, this->tracker_, this->owner_, true);
        this->transferBindingsTo(out);
        return out;
      }

      /**
       * Consume an owning input, including a named stream, into a linear chain.
       * The final stream (or its FlowSlot) owns every intermediate State;
       * fan-out from one owning intermediate is not supported. Borrowed roots
       * remain reusable, as they do not transfer State ownership.
       */
      template <typename R, typename ExprT> StateStream<R> map(const Expr<R, ExprT> &expr) const
      {
        PROFILE_SECTION("sMapExpr");
        if (!this->state_)
        {
          return StateStream<R>(0, this->tracker_, this->owner_);
        }
        assert(this->owner_ && "StateStream::map(expr) requires IStateOwner");
        MapSlotExprEval<T, R, ExprT> *eval = new MapSlotExprEval<T, R, ExprT>(this->state_, expr);
        // As in mapper map, bindings evaluate updates; adoption retains storage
        // and a tracker row, but must not install another evaluation route.
        ::loka::core::DerivedState<R> *derived =
            new ::loka::core::DerivedState<R>(std::vector< ::loka::core::StateBase *>(), eval);
        this->adoptDerived(derived);
        this->bindRecompute(this->state_, derived);
        StateStream<R> out(derived, this->tracker_, this->owner_, true);
        this->transferBindingsTo(out);
        return out;
      }

      /**
       * Consume either owning input, including a named stream, into a linear chain.
       * The final stream (or its FlowSlot) owns every intermediate State;
       * fan-out from one owning intermediate is not supported. Borrowed roots
       * remain reusable, as they do not transfer State ownership.
       */
      template <typename U, typename Combiner>
      StateStream<typename Combiner::Result> combine(const StateStream<U> &other, const Combiner &combiner) const
      {
        PROFILE_SECTION("sComb");
        if (!this->state_ || !other.state_)
        {
          return StateStream<typename Combiner::Result>(0, this->tracker_, this->owner_);
        }
        assert(this->owner_ && this->owner_ == other.owner_ && "StateStream::combine requires same IStateOwner");
        PROFILE_SECTION("sCombEvalNew");
        CombineEval<T, U, typename Combiner::Result, Combiner> *eval =
            new CombineEval<T, U, typename Combiner::Result, Combiner>(this->state_, other.state_, combiner);
        PROFILE_SECTION("sCombDerNew");
        // Each input binding evaluates updates. Empty dependencies avoid a
        // duplicate tracker evaluation while retaining owner adoption.
        ::loka::core::DerivedState<typename Combiner::Result> *derived =
            new ::loka::core::DerivedState<typename Combiner::Result>(std::vector< ::loka::core::StateBase *>(), eval);
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
        PROFILE_SECTION("sSet");
        if (!this->state_)
        {
          return;
        }
        PROFILE_SECTION("sSetNew");
        SetBinding *binding = new SetBinding(this->state_, &target, forceUpdate);
        PROFILE_SECTION("sSetApply");
        binding->apply();
        PROFILE_SECTION("sSetBind");
        this->state_->deferBind(&SetBinding::ApplyThunk, binding);
        this->addBinding(this->state_, &SetBinding::ApplyThunk, binding, &SetBinding::Destroy);
      }

      /** Stop subscriptions and owned-State propagation, retaining destruction records.
          A callback already on the stack may finish. Broad owners may decline
          State withdrawal; the stream still disconnects its explicit bindings. */
      void withdraw()
      {
        for (size_t i = 0; i < this->bindings_.size(); ++i)
        {
          StateStreamBindingEntry &entry = this->bindings_[i];
          if (entry.source && entry.cb)
            entry.source->deferUnbind(entry.cb, entry.userData);
          entry.source = 0;
          entry.cb = 0;
          if (entry.ownedState && this->owner_)
            this->owner_->withdrawState(entry.ownedState);
        }
        if (this->ownsState_ && this->owner_ && this->state_)
          this->owner_->withdrawState(this->state_);
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
        for (size_t i = bindings_.size(); i > 0; --i)
        {
          if (bindings_[i - 1].ownedState && owner_)
            this->owner_->releaseState(bindings_[i - 1].ownedState);
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

      template <typename U> void transferBindingsTo(StateStream<U> &out) const
      {
        if (this->ownsState_)
        {
          StateStreamBindingEntry owned;
          owned.ownedState = this->state_;
          this->bindings_.push_back(owned);
          this->ownsState_ = false;
          this->state_ = 0;
        }
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
            : state_(state),
              mapper_(mapper)
        {
        }

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
            : state_(state),
              expr_(expr)
        {
        }

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
        CombineEval(::loka::core::State<A> *left, ::loka::core::State<B> *right, const Combiner &combiner)
            : left_(left),
              right_(right),
              combiner_(combiner)
        {
        }

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
        SetBinding(::loka::core::State<T> *state, ::loka::app::scene::NodeState<T> *target, bool forceUpdate)
            : state_(state),
              target_(target),
              forceUpdate_(forceUpdate)
        {
        }

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
        explicit RecomputeBinding(::loka::core::StateBase *state)
            : state_(state)
        {
        }

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

      void bindRecompute(::loka::core::StateBase *source, ::loka::core::StateBase *derived) const
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
      template <typename U> friend class StateStream;

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
      template <typename T> inline loka::dsl::StateStream<T> NodeState<T>::stream() const
      {
        return loka::dsl::StateStream<T>(this->state_, this->tracker_, this->owner_);
      }
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_DSL_STATESTREAM_HPP
