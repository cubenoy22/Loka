#ifndef DECLARA_HELLOWORLD_MAIN_COMPONENT_HPP
#define DECLARA_HELLOWORLD_MAIN_COMPONENT_HPP

#include <string>
#include <cassert>
#include "core/State.hpp"
#include "core2/scene/node/Group.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "BmiCalculatorComponent.hpp"

namespace helloworld
{
  class MainNode;
  typedef declara::core::scene::GroupPropsFor<MainNode> MainProps;

  class MainNode : public declara::core::scene::GroupNodeBase<MainProps>
  {
  public:
    MainProps props;
    MainNode(const MainProps &p)
        : declara::core::scene::GroupNodeBase<MainProps>(MainProps(p)),
          props(p),
          message_(0),
          toggleEvent_()
    {
      // State is initialized lazily in composeNode via NodeComposition::useState.
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      message_ = &c.useState<std::string>("Hello, Loka!");
      this->bindForUi(toggleEvent_, this, &MainNode::toggleMessage);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(
          VStack()
          << Text("Loka Sample")
          << Text(message_)
          << Button("Toggle Message", &toggleEvent_)
          << BmiCalculator());
    }

  private:
    void toggleMessage()
    {
      if (!message_)
      {
        return;
      }
      const AttachedContext *ctx = this->attachedContext();
      if (!ctx)
      {
        return;
      }
      std::string next = "Hello, Loka!";
      if (message_->get() == "Hello, Loka!")
      {
        next = "Loka says hi!";
      }
      {
        StateTrackerGuard _(ctx->boundary()->tracker());
        message_->set(next, true);
      }

      {
        StateTrackerGuard _(ctx->window()->getTracker());
        const std::string title = ctx->window()->titleState().get();

        if (title == "LokaSample")
        {
          ctx->window()->titleState().set("LokaSample*");
        }
        else
        {
          ctx->window()->titleState().set("LokaSample");
        }
      }
    }

    MutableState<std::string> *message_;
    EmitterState toggleEvent_;
  };

  inline declara::core::scene::NodeDefinition<MainProps, MainNode> Main()
  {
    return declara::core::scene::NodeDefinition<MainProps, MainNode>();
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_MAIN_COMPONENT_HPP
