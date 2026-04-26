#ifndef LOKA_APP_SCENE_FLOW_SLOT_HPP
#define LOKA_APP_SCENE_FLOW_SLOT_HPP

#include <cassert>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <typename FlowT>
      class FlowSlot
      {
      public:
        FlowSlot() : flow_(0) {}
        ~FlowSlot() { this->clear(); }

        bool isValid() const { return flow_ != 0; }

        void clear()
        {
          if (flow_)
          {
            delete flow_;
            flow_ = 0;
          }
        }

        void set(const FlowT &flow)
        {
          this->clear();
          flow_ = new FlowT(flow);
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
