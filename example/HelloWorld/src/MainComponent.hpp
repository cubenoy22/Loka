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
      toggleEvent_.bind(&MainNode::ToggleMessageThunk, this, false);
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
    static void ToggleMessageThunk(void *userData)
    {
      MainNode *self = static_cast<MainNode *>(userData);
      if (self)
      {
        self->toggleMessage();
      }
    }

    void toggleMessage()
    {
      if (!message_)
      {
        return;
      }
      std::string next = "Hello, Loka!";
      if (message_->get() == "Hello, Loka!")
      {
        next = "Loka says hi!";
      }
      {
        StateTrackerGuard _(this->boundary()->tracker());
        message_->set(next, true);
      }

      {
        StateTrackerGuard _(this->window()->getTracker());
        const std::string title = this->window()->titleState().get();

        if (title == "LokaSample")
        {
          this->window()->titleState().set("LokaSample*");
        }
        else
        {
          this->window()->titleState().set("LokaSample");
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
