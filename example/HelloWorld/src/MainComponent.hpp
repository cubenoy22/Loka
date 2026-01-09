#ifndef LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
#define LOKA_HELLOWORLD_MAIN_COMPONENT_HPP

#include "core/util/StateTrackerGuard.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/node/DynamicComposition.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/PopupMenu.hpp"
#include "BmiCalculatorComponent.hpp"
#include "ToolboxControlSlots.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"

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
          fruitIndex_(),
          fruitMessage_(),
          toggleEvent_()
    {
      this->fruits_.push_back(loka::core::String::Literal("Apple"));
      this->fruits_.push_back(loka::core::String::Literal("Banana"));
      this->fruits_.push_back(loka::core::String::Literal("Cherry"));
      this->fruits_.push_back(loka::core::String::Literal("Grape"));
      // State is initialized lazily in composeNode via NodeComposition::useState.
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      this->message_ = c.useState<loka::core::String>(loka::core::String::Literal("Hello, Loka!"));
      this->fruitIndex_ = c.useState<int>(0);
      this->fruitMessage_ = c.useState<loka::core::String>(loka::core::String::Literal("You chose Apple."));
      this->bindForUi(toggleEvent_, this, &MainNode::toggleMessage);
      this->bindForUi(fruitChangedEvent_, this, &MainNode::handleFruitChanged);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(
          VStack()
          << Text("Loka Sample")
          << Text(this->message_)
          << Button("Add +", &toggleEvent_).toolboxControl(kToolboxControlAddButton)
          << BmiCalculator()
          << Text("Fruit Picker")
          << PopupMenu(this->fruits_).selectedIndex(this->fruitIndex_).onChange(&fruitChangedEvent_)
          << Text(this->fruitMessage_));
    }

  private:
    void handleFruitChanged()
    {
      if (!this->fruitIndex_.isValid() || !this->fruitMessage_.isValid() || this->fruits_.empty())
      {
        return;
      }
      int index = this->fruitIndex_.get();
      if (index < 0 || static_cast<std::size_t>(index) >= this->fruits_.size())
      {
        index = 0;
      }
      loka::core::String next = loka::core::String::Literal("You chose ") + this->fruits_[static_cast<std::size_t>(index)] + ".";
      this->fruitMessage_.set(next, true);
    }

    void toggleMessage()
    {
      if (!this->message_.isValid())
      {
        return;
      }
      const AttachedContext *ctx = this->attachedContext();
      if (!ctx)
      {
        return;
      }
      loka::core::String next = this->message_.get() + " +Loka";
      this->message_.set(next, true);

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
    declara::core::scene::BoundState<int> fruitIndex_;
    declara::core::scene::BoundState<loka::core::String> fruitMessage_;
    loka::Vector<loka::core::String> fruits_;
    EmitterState toggleEvent_;
    EmitterState fruitChangedEvent_;
  };

  inline declara::core::scene::NodeDefinition<MainProps, MainNode> Main()
  {
    return declara::core::scene::NodeDefinition<MainProps, MainNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
