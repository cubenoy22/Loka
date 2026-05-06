#ifndef LOKA_APP_SCENE_FLOW_SLOT_HPP
#define LOKA_APP_SCENE_FLOW_SLOT_HPP

#include <cassert>
#include "app/scene/state/NodeState.hpp"
#include "loka/dsl/Flow.hpp"
#include "loka/dsl/StateStream.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace flow_slot_detail
      {
        template <typename FlowT>
        inline void releaseOwnedFlow(FlowT *)
        {
        }

        template <typename T>
        inline void releaseOwnedFlow(loka::dsl::StateStream<T> *stream)
        {
          if (stream)
          {
            stream->releaseOwnedState();
          }
        }
      }

      template <typename FlowT>
      class FlowSlot
      {
      public:
        FlowSlot() : flow_(0) {}
        ~FlowSlot() { this->clear(); }

        bool isValid() const { return flow_ != 0; }

        FlowSlot &set(const FlowT &flow)
        {
          this->clear();
          flow_ = new FlowT(flow);
          return *this;
        }

        void clear()
        {
          if (flow_)
          {
            flow_slot_detail::releaseOwnedFlow(flow_);
            delete flow_;
            flow_ = 0;
          }
        }

        FlowSlot &withTracker(loka::core::PushStateTracker *tracker)
        {
          assert(flow_ && "FlowSlot::withTracker requires a flow");
          flow_->withTracker(tracker);
          return *this;
        }

        template <typename InT>
        FlowSlot &bindTrigger(loka::core::State<InT> *source)
        {
          assert(flow_ && "FlowSlot::bindTrigger requires a flow");
          flow_->bindTrigger(source);
          return *this;
        }

        template <typename InT>
        FlowSlot &bindTrigger(const NodeState<InT> &source)
        {
          assert(flow_ && "FlowSlot::bindTrigger requires a flow");
          assert(source.isValid() && "FlowSlot::bindTrigger requires a valid NodeState source");
          flow_->bindTrigger(source.dangerouslyMutableState());
          return *this;
        }

        template <typename TargetT>
        FlowSlot &bindTo(TargetT &target, bool forceUpdate = false)
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

        FlowT *get() { return flow_; }
        const FlowT *get() const { return flow_; }

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
        FlowSlot(const FlowSlot &);
        FlowSlot &operator=(const FlowSlot &);

        FlowT *flow_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_APP_SCENE_FLOW_SLOT_HPP
