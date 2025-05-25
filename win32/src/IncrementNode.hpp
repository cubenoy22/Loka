#ifndef DECLARA_INCREMENT_NODE_HPP
#define DECLARA_INCREMENT_NODE_HPP

#include "core/SceneNode.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/SceneNodeFactory.hpp"
#include "core/util/AutoTransactionGuard.hpp"

struct IncrementNodeTypeTag
{
};

struct IncrementNodeProps : public NodePropsBase<IncrementNodeProps>
{
  typedef IncrementNodeTypeTag TypeTag;
  MutableState<int> *count;
  EmitterState *trigger;
  StateTracker *tracker;
  IncrementNodeProps(MutableState<int> *c = 0, EmitterState *t = 0, StateTracker *tr = 0)
      : count(c), trigger(t), tracker(tr) {}
  virtual bool operator<(const PropsBase &rhs) const
  {
    const IncrementNodeProps *p = dynamic_cast<const IncrementNodeProps *>(&rhs);
    if (!p)
      return false;
    if (count != p->count)
      return count < p->count;
    if (trigger != p->trigger)
      return trigger < p->trigger;
    return tracker < p->tracker;
  }
  int hash() const
  {
    // ポインタ値をintにキャストして組み合わせる（C++98範囲で簡易実装）
    int h = 17;
    h = h * 31 + (int)(long)count;
    h = h * 31 + (int)(long)trigger;
    h = h * 31 + (int)(long)tracker;
    return h;
  }
  static SceneNode *createNode(const IncrementNodeProps &props);
};

class IncrementNode : public SceneNode
{
public:
  typedef IncrementNodeTypeTag TypeTag;
  IncrementNodeProps props;
  IncrementNode(const IncrementNodeProps &p)
      : SceneNode(Reuse_Singleton), props(p)
  {
    assert(props.tracker && "StateTracker must not be null");
    if (props.trigger)
      props.trigger->deferBind(&IncrementNode::onTrigger, this);
  }

  static void onTrigger(void *userData)
  {
    IncrementNode *self = static_cast<IncrementNode *>(userData);
    AutoTransactionGuard _(self->props.tracker);
    self->props.count->set(self->props.count->get() + 1);
  }
};

inline SceneNode *IncrementNodeProps::createNode(const IncrementNodeProps &props) { return new IncrementNode(props); }

typedef NodeDefinition<IncrementNodeProps, IncrementNode> IncrementNodeDefinition;

#endif // DECLARA_INCREMENT_NODE_HPP
