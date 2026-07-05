#ifndef LOKA_APP_SCENE_STATE_FLOW_SLOT_HPP
#define LOKA_APP_SCENE_STATE_FLOW_SLOT_HPP

#include <cassert>
#include "app/scene/state/NodeState.hpp"
#include "dsl/flow/Flow.hpp"
#include "dsl/stream/StateStream.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace flow_slot_detail
      {
        template <typename FlowT> inline void releaseOwnedFlow(FlowT *) {}

        template <typename FlowT> inline void clearExecutionHooks(FlowT *)
        {
        }

        template <typename InT, typename OutT> inline void clearExecutionHooks(loka::dsl::FlowChain<InT, OutT> *flow)
        {
          if (flow)
          {
            flow->clearExecutionHooks();
          }
        }

        template <typename FlowT> inline void destroyFlow(FlowT *flow)
        {
          if (!flow)
          {
            return;
          }
          clearExecutionHooks(flow);
          releaseOwnedFlow(flow);
          delete flow;
        }

        template <typename T> inline void releaseOwnedFlow(loka::dsl::StateStream<T> *stream)
        {
          if (stream)
          {
            stream->releaseOwnedState();
          }
        }

        template <typename FlowT>
        inline void attachExecutionHooks(FlowT *, void (*)(void *, void *), void (*)(void *, void *), void *)
        {
        }

        template <typename InT, typename OutT>
        inline void attachExecutionHooks(loka::dsl::FlowChain<InT, OutT> *flow,
                                         void (*beginFn)(void *, void *),
                                         void (*endFn)(void *, void *),
                                         void *user)
        {
          if (flow)
          {
            flow->setExecutionHooks(beginFn, endFn, user);
          }
        }
      } // namespace flow_slot_detail

      template <typename FlowT> class FlowSlot
      {
      public:
        FlowSlot()
            : flow_(0),
              runState_(new RunState())
        {
        }
        ~FlowSlot()
        {
          this->clear();
          if (this->runState_)
          {
            this->runState_->ownerAlive_ = false;
            if (this->runState_->runningDepth_ == 0)
            {
              delete this->runState_;
            }
            this->runState_ = 0;
          }
        }

        bool isValid() const
        {
          return flow_ != 0;
        }

        FlowSlot &set(const FlowT &flow)
        {
          FlowT *next = new FlowT(flow);
          this->attachExecutionHooks(next);
          if (this->shouldDeferRelease(this->flow_))
          {
            assert(this->runState_->deferredFlow_ == 0 && "FlowSlot::set already has a deferred running flow");
            this->runState_->deferredFlow_ = this->flow_;
            this->flow_ = next;
            return *this;
          }
          this->clear();
          this->flow_ = next;
          return *this;
        }

        void clear()
        {
          if (this->shouldDeferRelease(this->flow_))
          {
            assert(this->runState_->deferredFlow_ == 0 && "FlowSlot::clear already has a deferred running flow");
            this->runState_->deferredFlow_ = this->flow_;
            this->flow_ = 0;
            return;
          }
          if (this->flow_)
          {
            flow_slot_detail::destroyFlow(this->flow_);
            this->flow_ = 0;
          }
        }

        FlowSlot &withTracker(loka::core::PushStateTracker *tracker)
        {
          assert(flow_ && "FlowSlot::withTracker requires a flow");
          flow_->withTracker(tracker);
          return *this;
        }

        template <typename InT> FlowSlot &bindTrigger(loka::core::State<InT> *source)
        {
          assert(flow_ && "FlowSlot::bindTrigger requires a flow");
          flow_->bindTrigger(source);
          return *this;
        }

        template <typename InT> FlowSlot &bindTrigger(const NodeState<InT> &source)
        {
          assert(flow_ && "FlowSlot::bindTrigger requires a flow");
          assert(source.isValid() && "FlowSlot::bindTrigger requires a valid NodeState source");
          flow_->bindTrigger(source.dangerouslyMutableState());
          return *this;
        }

        template <typename TargetT> FlowSlot &bindTo(TargetT &target, bool forceUpdate = false)
        {
          assert(flow_ && "FlowSlot::bindTo requires a flow");
          flow_->set(target, forceUpdate);
          return *this;
        }

        bool run() const
        {
          assert(flow_ && "FlowSlot::run requires a flow");
          return flow_->run();
        }

        loka::dsl::FlowRunResult runResult() const
        {
          assert(flow_ && "FlowSlot::runResult requires a flow");
          return flow_->runResult();
        }

        bool resume(int stepId) const
        {
          assert(flow_ && "FlowSlot::resume requires a flow");
          return flow_->resume(stepId);
        }

        loka::dsl::FlowRunResult resumeResult(int stepId) const
        {
          assert(flow_ && "FlowSlot::resumeResult requires a flow");
          return flow_->resumeResult(stepId);
        }

        FlowSlot &cancel()
        {
          assert(flow_ && "FlowSlot::cancel requires a flow");
          flow_->cancel();
          return *this;
        }

        FlowSlot &clearCancel()
        {
          assert(flow_ && "FlowSlot::clearCancel requires a flow");
          flow_->clearCancel();
          return *this;
        }

        FlowT *get()
        {
          return flow_;
        }
        const FlowT *get() const
        {
          return flow_;
        }

        FlowT &dangerouslyUnwrap()
        {
          assert(flow_ && "FlowSlot::dangerouslyUnwrap requires a flow");
          return *flow_;
        }

        const FlowT &dangerouslyUnwrap() const
        {
          assert(flow_ && "FlowSlot::dangerouslyUnwrap requires a flow");
          return *flow_;
        }

      private:
        struct RunState
        {
          RunState()
              : runningDepth_(0),
                runningFlow_(0),
                deferredFlow_(0),
                ownerAlive_(true)
          {
          }

          int runningDepth_;
          FlowT *runningFlow_;
          FlowT *deferredFlow_;
          bool ownerAlive_;
        };

        static void onFlowRunBegin(void *flow, void *user)
        {
          RunState *state = static_cast<RunState *>(user);
          FlowT *runningFlow = static_cast<FlowT *>(flow);
          assert(state != 0 && "FlowSlot::onFlowRunBegin requires run state");
          if (state->runningDepth_ == 0)
          {
            state->runningFlow_ = runningFlow;
          }
          else
          {
            assert(state->runningFlow_ == runningFlow && "FlowSlot::onFlowRunBegin switched running flow mid-run");
          }
          ++state->runningDepth_;
        }

        static void onFlowRunEnd(void *flow, void *user)
        {
          RunState *state = static_cast<RunState *>(user);
          FlowT *runningFlow = static_cast<FlowT *>(flow);
          assert(state != 0 && "FlowSlot::onFlowRunEnd requires run state");
          assert(state->runningDepth_ > 0 && "FlowSlot::onFlowRunEnd underflow");
          assert(state->runningFlow_ == runningFlow && "FlowSlot::onFlowRunEnd running flow mismatch");
          --state->runningDepth_;
          if (state->runningDepth_ == 0)
          {
            state->runningFlow_ = 0;
            if (state->deferredFlow_ != 0)
            {
              flow_slot_detail::destroyFlow(state->deferredFlow_);
              state->deferredFlow_ = 0;
            }
            if (!state->ownerAlive_)
            {
              delete state;
            }
          }
        }

        bool shouldDeferRelease(FlowT *flow) const
        {
          return flow != 0 && this->runState_ != 0 && this->runState_->runningDepth_ > 0 &&
                 this->runState_->runningFlow_ == flow;
        }

        void attachExecutionHooks(FlowT *flow)
        {
          flow_slot_detail::attachExecutionHooks(flow, &FlowSlot::onFlowRunBegin, &FlowSlot::onFlowRunEnd, this->runState_);
        }

        FlowSlot(const FlowSlot &);
        FlowSlot &operator=(const FlowSlot &);

        FlowT *flow_;
        RunState *runState_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_APP_SCENE_STATE_FLOW_SLOT_HPP
