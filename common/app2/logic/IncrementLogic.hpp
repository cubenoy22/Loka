#ifndef DECLARA_APP2_LOGIC_INCREMENTLOGIC_HPP
#define DECLARA_APP2_LOGIC_INCREMENTLOGIC_HPP

#include <cstdio>
#include <string>
#include "core/State.hpp"
#include "core2/scene/Node.hpp"

namespace declara
{
  namespace app
  {
    struct IncrementLogicTypeTag
    {
    };

    class IncrementLogicNode;

    struct IncrementLogicProps : public core::scene::NodePropsBase<IncrementLogicProps>
    {
      typedef IncrementLogicTypeTag TypeTag;
      typedef IncrementLogicNode NodeType;
      MutableState<std::string> *labelState;
      EmitterState *trigger;
      IncrementLogicProps() : labelState(0), trigger(0) {}

      IncrementLogicProps &setLabel(MutableState<std::string> *state)
      {
        labelState = state;
        return *this;
      }
      IncrementLogicProps &setTrigger(EmitterState *e)
      {
        trigger = e;
        return *this;
      }

      bool operator<(const core::scene::PropsBase &rhs) const
      {
        const IncrementLogicProps *p = dynamic_cast<const IncrementLogicProps *>(&rhs);
        if (!p)
          return false;
        if (labelState != p->labelState)
          return labelState < p->labelState;
        return trigger < p->trigger;
      }
    };

    class IncrementLogicNode : public core::scene::Node
    {
    public:
      typedef IncrementLogicTypeTag TypeTag;
      IncrementLogicProps props;
      IncrementLogicNode(const IncrementLogicProps &p) : props(p), count_(0)
      {
        if (props.trigger)
        {
          props.trigger->deferBind(&IncrementLogicNode::onTriggered, this);
        }
      }
      virtual ~IncrementLogicNode()
      {
        if (props.trigger)
        {
          props.trigger->deferUnbind(&IncrementLogicNode::onTriggered, this);
        }
      }

    private:
      int count_;
      static void onTriggered(void *userData)
      {
        IncrementLogicNode *self = static_cast<IncrementLogicNode *>(userData);
        if (!self)
          return;
        self->count_ += 1;
        self->updateLabel(self->count_);
      }

      void updateLabel(int count)
      {
        if (!props.labelState)
          return;
        char buf[64];
        sprintf(buf, "Clicked %d", count);
        props.labelState->set(std::string(buf), true);
      }
    };

    struct IncrementLogicDefinition : public core::scene::NodeDefinition<IncrementLogicProps, IncrementLogicNode>
    {
      IncrementLogicDefinition() : NodeDefinition() {}
      IncrementLogicDefinition(const IncrementLogicProps &p) : NodeDefinition(p) {}
    };

    typedef IncrementLogicDefinition IncrementLogic;

  } // namespace app
} // namespace declara

#endif // DECLARA_APP2_LOGIC_INCREMENTLOGIC_HPP
