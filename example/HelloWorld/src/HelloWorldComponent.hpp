#ifndef LOKA_HELLOWORLD_COMPONENT_HPP
#define LOKA_HELLOWORLD_COMPONENT_HPP

#include "core2/scene/node/Group.hpp"
#include "core2/scene/BoundState.hpp"
#include "loka/core/String.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "ChangeContextButton.hpp"
#include "RootBoundary.hpp"
#include <cstring>

namespace helloworld
{
  class HelloWorldNode;

  typedef loka::core::scene::GroupPropsFor<HelloWorldNode> HelloWorldProps;

  class HelloWorldNode : public loka::core::scene::GroupNodeBase<HelloWorldProps>, public RootBoundary
  {
  public:
    HelloWorldProps props;
    HelloWorldNode(const HelloWorldProps &p)
        : loka::core::scene::GroupNodeBase<HelloWorldProps>(HelloWorldProps(p)), props(p), message_() {}

    virtual void attachNode(loka::core::scene::NodeComposition &c)
    {
      message_ = c.useState<loka::core::String>(loka::core::String::Literal("Hello, Loka!"));
    }

    virtual void composeNode(loka::core::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(
          VStack()
          << Text("Loka Prototype")
          << Text(message_)
          << ChangeContextButtonNode());
    }

    virtual loka::core::scene::BoundState<loka::core::String> &messageState()
    {
      return message_;
    }

    virtual void *queryInterface(const char *name)
    {
      if (name && std::strcmp(name, RootBoundary::kInterfaceName()) == 0)
      {
        return static_cast<RootBoundary *>(this);
      }
      return loka::core::scene::GroupNodeBase<HelloWorldProps>::queryInterface(name);
    }

  private:
    loka::core::scene::BoundState<loka::core::String> message_;
  };

  inline loka::core::scene::NodeDefinition<HelloWorldProps, HelloWorldNode> HelloWorld()
  {
    return loka::core::scene::NodeDefinition<HelloWorldProps, HelloWorldNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_COMPONENT_HPP
