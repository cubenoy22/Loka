#ifndef LOKA_HELLOWORLD_MAIN_NODE_HPP
#define LOKA_HELLOWORLD_MAIN_NODE_HPP

#include "core/util/StateTrackerGuard.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/node/StaticComposition.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include "BmiCalculatorComponent.hpp"

namespace helloworld
{
  static const loka::core::String kFruitItems[] =
      {
          loka::core::String::Literal("Apple"),
          loka::core::String::Literal("Banana"),
          loka::core::String::Literal("Cherry"),
          loka::core::String::Literal("Grape"),
      };
  static const std::size_t kFruitItemCount = sizeof(kFruitItems) / sizeof(kFruitItems[0]);

  class MainNode;
  typedef declara::core::scene::StaticCompositionPropsFor<MainNode> MainProps;

  class MainNode : public declara::core::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    struct FruitPopupLabel
    {
      typedef loka::core::String Result;
      loka::core::String operator()(const loka::core::String &value) const { return value; }
    };

    MainNode(const MainProps &p)
        : declara::core::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
          message_(),
          fruitIndex_(),
          fruitMessage_(),
          toggleEvent_(),
          fruitChangedEvent_(),
          bmiCalculator_()
    {
      this->fruits_.assign(kFruitItems, kFruitItemCount);
      // State is initialized in attachNode via NodeComposition::declareStates.
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      declara::core::scene::NodeComposition::StateBatch states = c.declareStates();
      states.state(message_, loka::core::String::Literal("Hello, Loka!"))
          .state(fruitIndex_, 0)
          .state(fruitMessage_, loka::core::String::Literal("You chose Apple."));
      this->bindForUi(toggleEvent_, this, &MainNode::toggleMessage);
      this->bindForUi(fruitChangedEvent_, this, &MainNode::handleFruitChanged);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c);

    declara::core::scene::BoundState<loka::core::String> &messageState() { return message_; }
    declara::core::scene::BoundState<int> &fruitIndexState() { return fruitIndex_; }
    declara::core::scene::BoundState<loka::core::String> &fruitMessageState() { return fruitMessage_; }
    loka::Vector<loka::core::String> &fruits() { return fruits_; }
    EmitterState &toggleEvent() { return toggleEvent_; }
    EmitterState &fruitChangedEvent() { return fruitChangedEvent_; }
    BmiCalculatorComponent &bmiCalculator() { return bmiCalculator_; }

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
    BmiCalculatorComponent bmiCalculator_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_NODE_HPP
