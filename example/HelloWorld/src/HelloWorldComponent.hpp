#ifndef LOKA_HELLOWORLD_COMPONENT_HPP
#define LOKA_HELLOWORLD_COMPONENT_HPP

#include "core2/scene/node/Group.hpp"
#include "core2/scene/BoundState.hpp"
#include "loka/core/String.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "ChangeContextButton.hpp"
#include "RootBoundary.hpp"

namespace helloworld
{
  class HelloWorldNode;

  typedef declara::core::scene::GroupPropsFor<HelloWorldNode> HelloWorldProps;

  class HelloWorldNode : public declara::core::scene::GroupNodeBase<HelloWorldProps>, public RootBoundary
  {
  public:
    HelloWorldProps props;
    HelloWorldNode(const HelloWorldProps &p)
        : declara::core::scene::GroupNodeBase<HelloWorldProps>(HelloWorldProps(p)), props(p), message_() {}

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      message_ = c.useState<loka::core::String>(loka::core::String::Literal("Hello, Loka!"));
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(
          VStack()
          << Text("Loka Prototype")
          << Text(message_)
          << ChangeContextButtonNode());
    }

    virtual declara::core::scene::BoundState<loka::core::String> &messageState()
    {
      return message_;
    }

  private:
    declara::core::scene::BoundState<loka::core::String> message_;
  };

  inline declara::core::scene::NodeDefinition<HelloWorldProps, HelloWorldNode> HelloWorld()
  {
    return declara::core::scene::NodeDefinition<HelloWorldProps, HelloWorldNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_COMPONENT_HPP
