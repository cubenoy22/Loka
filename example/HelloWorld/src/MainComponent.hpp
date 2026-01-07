#ifndef LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
#define LOKA_HELLOWORLD_MAIN_COMPONENT_HPP

#include "core/util/StateTrackerGuard.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/node/DynamicComposition.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "BmiCalculatorComponent.hpp"
#include "loka/core/String.hpp"

namespace helloworld
{
  class MainNode;
  typedef declara::core::scene::DynamicCompositionPropsFor<MainNode> MainProps;

  class MainNode : public declara::core::scene::DynamicCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : declara::core::scene::DynamicCompositionNodeFor<MainNode>(MainProps(p)),
          message_(),
          toggleEvent_()
    {
      // State is initialized lazily in composeNode via NodeComposition::useState.
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      message_ = c.useState<loka::core::String>(loka::core::String::Literal("Hello, Loka!"));
      this->bindForUi(toggleEvent_, this, &MainNode::toggleMessage);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(
          VStack()
          << Text("Loka Sample")
          << Text(message_)
          << Button("Add +", &toggleEvent_)
          << BmiCalculator());
    }

  private:
    void toggleMessage()
    {
      if (!message_.isValid())
      {
        return;
      }
      const AttachedContext *ctx = this->attachedContext();
      if (!ctx)
      {
        return;
      }
      loka::core::String next = message_.get() + " +Loka";
      message_.set(next, true);

      {
        StateTrackerGuard _(ctx->window()->getTracker());
        const loka::core::String title = ctx->window()->titleState().get();

        if (title.equals(loka::core::String::Literal("LokaSample")))
        {
          ctx->window()->titleState().set(loka::core::String::Literal("LokaSample*"));
        }
        else
        {
          ctx->window()->titleState().set(loka::core::String::Literal("LokaSample"));
        }
      }
    }

    declara::core::scene::BoundState<loka::core::String> message_;
    EmitterState toggleEvent_;
  };

  inline declara::core::scene::NodeDefinition<MainProps, MainNode> Main()
  {
    return declara::core::scene::NodeDefinition<MainProps, MainNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
