#ifndef LOKA_HELLOWORLD_MAIN_NODE_HPP
#define LOKA_HELLOWORLD_MAIN_NODE_HPP

#include "core/util/StateTrackerGuard.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/Component.hpp"
#include "core2/scene/node/StaticComposition.hpp"
#include "app/Button.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
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

    struct LeftPanelState
    {
      declara::core::scene::BoundState<loka::core::String> message;
      declara::core::EmitterState toggleEvent;
      BmiCalculatorComponent bmiCalculator;

      LeftPanelState() : message(), toggleEvent(), bmiCalculator() {}

      declara::core::scene::BoundState<loka::core::String> &messageState() { return message; }
      declara::core::EmitterState &toggleEventState() { return toggleEvent; }
      BmiCalculatorComponent &bmiCalculatorState() { return bmiCalculator; }
    };

    struct RightPanelState
    {
      declara::core::scene::BoundState<int> fruitIndex;
      declara::core::scene::BoundState<loka::core::String> fruitMessage;
      loka::Vector<loka::core::String> fruits;
      declara::core::EmitterState fruitChangedEvent;

      RightPanelState() : fruitIndex(), fruitMessage(), fruits(), fruitChangedEvent() {}

      declara::core::scene::BoundState<int> &fruitIndexState() { return fruitIndex; }
      declara::core::scene::BoundState<loka::core::String> &fruitMessageState() { return fruitMessage; }
      loka::Vector<loka::core::String> &fruitsState() { return fruits; }
      declara::core::EmitterState &fruitChangedEventState() { return fruitChangedEvent; }
    };

    MainNode(const MainProps &p)
        : declara::core::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
          left_(),
          right_()
    {
      this->right_.fruits.assign(kFruitItems, kFruitItemCount);
      // State is initialized in attachNode via NodeComposition::declareStates.
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      declara::core::scene::NodeComposition::StateBatch states = c.declareStates();
      states.state(left_.message, loka::core::String::Literal("Hello, Loka!"))
          .state(right_.fruitIndex, 0)
          .state(right_.fruitMessage, loka::core::String::Literal("You chose Apple."));
      this->bindForUi(left_.toggleEvent, this, &MainNode::toggleMessage);
      this->bindForUi(right_.fruitChangedEvent, this, &MainNode::handleFruitChanged);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c);

    LeftPanelState &left() { return left_; }
    RightPanelState &right() { return right_; }

  private:
    void handleFruitChanged()
    {
      if (!this->right_.fruitIndex.isValid() || !this->right_.fruitMessage.isValid() || this->right_.fruits.empty())
      {
        return;
      }
      int index = this->right_.fruitIndex.get();
      if (index < 0 || static_cast<std::size_t>(index) >= this->right_.fruits.size())
      {
        index = 0;
      }
      loka::core::String next = loka::core::String::Literal("You chose ") + this->right_.fruits[static_cast<std::size_t>(index)] + ".";
      this->right_.fruitMessage.set(next, true);
    }

    void toggleMessage()
    {
      if (!this->left_.message.isValid())
      {
        return;
      }
      const AttachedContext *ctx = this->attachedContext();
      if (!ctx)
      {
        return;
      }
      loka::core::String next = this->left_.message.get() + " +Loka";
      this->left_.message.set(next, true);

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

    LeftPanelState left_;
    RightPanelState right_;
  };

  class MainLeftPanelComponent
  {
  public:
    explicit MainLeftPanelComponent(MainNode *owner) : owner_(owner) {}

    void attachNode(declara::core::scene::NodeComposition &) {}

    void composeNode(declara::core::scene::NodeComposition &, declara::core::scene::INestableDefinition &parent)
    {
      using namespace declara::app;
      if (!owner_)
      {
        return;
      }
      VStack column;
      column
          << Text("Loka Sample")
          << Text(owner_->left().messageState())
          << Button("Add +", &owner_->left().toggleEventState())
          << declara::core::scene::LightComponent(owner_->left().bmiCalculatorState());
      parent << column;
    }

  private:
    MainNode *owner_;
  };

  class MainRightPanelComponent
  {
  public:
    explicit MainRightPanelComponent(MainNode *owner) : owner_(owner) {}

    void attachNode(declara::core::scene::NodeComposition &) {}

    void composeNode(declara::core::scene::NodeComposition &, declara::core::scene::INestableDefinition &parent)
    {
      using namespace declara::app;
      if (!owner_)
      {
        return;
      }
      VStack column;
      column
          << Text("Fruit Picker")
          << PopupMenu(owner_->right().fruitsState().map<loka::core::String>(MainNode::FruitPopupLabel()))
                 .selectedIndex(owner_->right().fruitIndexState())
                 .onChange(&owner_->right().fruitChangedEventState())
          << Text(owner_->right().fruitMessageState());
      parent << column;
    }

  private:
    MainNode *owner_;
  };

  inline void MainNode::composeNode(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    MainLeftPanelComponent leftPanel(this);
    MainRightPanelComponent rightPanel(this);
    c.declare(
        HStack()
        << declara::core::scene::LightComponent(leftPanel)
        << declara::core::scene::LightComponent(rightPanel));
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_NODE_HPP
