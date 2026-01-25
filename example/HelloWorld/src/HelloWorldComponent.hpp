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

  typedef loka::app::scene::GroupPropsFor<HelloWorldNode> HelloWorldProps;

  class HelloWorldNode : public loka::app::scene::GroupNodeBase<HelloWorldProps>, public RootBoundary
  {
  public:
    HelloWorldProps props;
    HelloWorldNode(const HelloWorldProps &p)
        : loka::app::scene::GroupNodeBase<HelloWorldProps>(HelloWorldProps(p)), props(p), message_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      message_ = c.useState<loka::core::String>(loka::core::String::Literal("Hello, Loka!"));
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(
          VStack()
          << Text("Loka Prototype")
          << Text(message_)
          << ChangeContextButtonNode());
    }

    virtual loka::app::scene::BoundState<loka::core::String> &messageState()
    {
      return message_;
    }

    virtual void *queryInterface(const char *name)
    {
      if (name && std::strcmp(name, RootBoundary::kInterfaceName()) == 0)
      {
        return static_cast<RootBoundary *>(this);
      }
      return loka::app::scene::GroupNodeBase<HelloWorldProps>::queryInterface(name);
    }

  private:
    loka::app::scene::BoundState<loka::core::String> message_;
  };

  inline loka::app::scene::NodeDefinition<HelloWorldProps, HelloWorldNode> HelloWorld()
  {
    return loka::app::scene::NodeDefinition<HelloWorldProps, HelloWorldNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_COMPONENT_HPP
