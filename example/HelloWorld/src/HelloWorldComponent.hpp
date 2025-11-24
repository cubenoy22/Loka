#ifndef DECLARA_HELLOWORLD_COMPONENT_HPP
#define DECLARA_HELLOWORLD_COMPONENT_HPP

#include "core2/scene/node/ComposableNode.hpp"

namespace helloworld
{
  struct HelloWorldTypeTag
  {
  };

  struct HelloWorldProps : public declara::core::scene::NodePropsBase<HelloWorldProps>
  {
    typedef HelloWorldTypeTag TypeTag;
    typedef class HelloWorldNode NodeType;

    bool operator<(const declara::core::scene::PropsBase &rhs) const
    {
      const HelloWorldProps *p = dynamic_cast<const HelloWorldProps *>(&rhs);
      if (!p)
        return false;
      return false;
    }
  };

  class HelloWorldNode : public declara::core::scene::ComposableNode
  {
  public:
    typedef HelloWorldTypeTag TypeTag;
    HelloWorldProps props;
    HelloWorldNode(const HelloWorldProps &p) : ComposableNode(), props(p) {}
    virtual ~HelloWorldNode() {}

    virtual void compose()
    {
      // TODO: move existing BMI/Error UI here
    }
  };

  struct HelloWorldDefinition : public declara::core::scene::NodeDefinition<HelloWorldProps, HelloWorldNode>
  {
    HelloWorldDefinition() : NodeDefinition<HelloWorldProps, HelloWorldNode>() {}
  };

  inline HelloWorldDefinition HelloWorld()
  {
    return HelloWorldDefinition();
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_COMPONENT_HPP
