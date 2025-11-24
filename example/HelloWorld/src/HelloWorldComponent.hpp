#ifndef DECLARA_HELLOWORLD_COMPONENT_HPP
#define DECLARA_HELLOWORLD_COMPONENT_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "app/Fragment.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"

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

  class HelloWorldNode : public declara::core::scene::StaticCompositionNode
  {
  public:
    typedef HelloWorldTypeTag TypeTag;
    HelloWorldProps props;
    HelloWorldNode(const HelloWorldProps &p) : StaticCompositionNode(HelloWorldProps(p)), props(p) {}
    virtual ~HelloWorldNode() {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(
          VStack()
          << Text(TextProps().setText("Hello, Declara!"))
          << Text(TextProps().setText("StaticCompositionNode prototype")));
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
