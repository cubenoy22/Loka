#ifndef DECLARA_HELLOWORLD_COMPONENT_HPP
#define DECLARA_HELLOWORLD_COMPONENT_HPP

#include <cassert>
#include "core2/scene/node/StaticComposition.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "ChangeContextButton.hpp"
#include "HelloWorldBoundary.hpp"

namespace helloworld
{
  struct HelloWorldTypeTag
  {
  };

  // StaticCompositionProps 拡張: 今は空。必要に応じて拡張可。
  struct HelloWorldProps : public declara::core::scene::StaticCompositionProps
  {
    typedef HelloWorldTypeTag TypeTag;
  };

  class HelloWorldNode : public declara::core::scene::StaticCompositionNode, public HelloWorldBoundary
  {
  public:
    typedef HelloWorldTypeTag TypeTag;
    HelloWorldProps props;
    HelloWorldNode(const HelloWorldProps &p)
        : declara::core::scene::StaticCompositionNode(HelloWorldProps(p)), props(p), message_(0) {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      if (!message_)
      {
        message_ = &c.useState<std::string>("Hello, Declara!");
      }

      using namespace declara::app;
      TextProps prototype;
      prototype.setText("StaticCompositionNode prototype");
      TextProps message;
      message.setText(message_);
      c.declare(
          VStack()
          << Text(prototype)
          << Text(message)
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

  struct HelloWorldDefinition : public declara::core::scene::BoundaryDefinition<HelloWorldProps, HelloWorldNode>
  {
    HelloWorldDefinition() : BoundaryDefinition<HelloWorldProps, HelloWorldNode>() {}
  };

  inline HelloWorldDefinition HelloWorld()
  {
    return HelloWorldDefinition();
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_COMPONENT_HPP
