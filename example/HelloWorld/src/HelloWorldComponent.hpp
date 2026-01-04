#ifndef LOKA_HELLOWORLD_COMPONENT_HPP
#define LOKA_HELLOWORLD_COMPONENT_HPP

#include <cassert>
#include "core2/scene/node/Group.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "ChangeContextButton.hpp"
#include "HelloWorldBoundary.hpp"

namespace helloworld
{
  class HelloWorldNode;

  typedef declara::core::scene::GroupPropsFor<HelloWorldNode> HelloWorldProps;

  class HelloWorldNode : public declara::core::scene::GroupNodeBase<HelloWorldProps>, public HelloWorldBoundary
  {
  public:
    HelloWorldProps props;
    HelloWorldNode(const HelloWorldProps &p)
        : declara::core::scene::GroupNodeBase<HelloWorldProps>(HelloWorldProps(p)), props(p), message_(0) {}

    virtual void prepareNode(declara::core::scene::NodeComposition &c)
    {
      message_ = &c.useState<std::string>("Hello, Loka!");
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

    virtual MutableState<std::string> &messageState()
    {
      assert(message_ && "HelloWorldNode messageState requires compose");
      return *message_;
    }

  private:
    MutableState<std::string> *message_;
  };

  inline declara::core::scene::NodeDefinition<HelloWorldProps, HelloWorldNode> HelloWorld()
  {
    return declara::core::scene::NodeDefinition<HelloWorldProps, HelloWorldNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_COMPONENT_HPP
