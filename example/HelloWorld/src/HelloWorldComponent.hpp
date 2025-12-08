#ifndef DECLARA_HELLOWORLD_COMPONENT_HPP
#define DECLARA_HELLOWORLD_COMPONENT_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "core2/scene/ContextDefinition.hpp"
#include "app/Fragment.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "HelloWorldContext.hpp"
#include "ChangeContextButton.hpp"

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

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::core::scene;
      HelloWorldContext &context = this->useContext(HelloWorldContextDefinition(), CONTEXT_PLACEMENT_BOUNDARY);
      c.useContext(HelloWorldContextDefinition(), context);

      using namespace declara::app;
      TextProps prototype;
      prototype.setText("StaticCompositionNode prototype");
      TextProps message;
      message.setText(&context.message);
      c.declare(
          VStack()
          << Text(prototype)
          << Text(message)
          << ChangeContextButtonNode());
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
