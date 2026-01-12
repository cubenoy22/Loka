#ifndef LOKA_HELLOWORLD_MAIN_NODE_HPP
#define LOKA_HELLOWORLD_MAIN_NODE_HPP

#include "core/util/StateTrackerGuard.hpp"
#include "core/Window.hpp"
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

  class MainLeftPanelComponent
  {
  public:
    explicit MainLeftPanelComponent(MainNode *owner);
    void attachNode(declara::core::scene::NodeComposition &c);
    void composeNode(declara::core::scene::NodeComposition &c);

  private:
    void toggleMessage();

    MainNode *owner_;
    bool initialized_;
    declara::core::scene::BoundState<loka::core::String> message_;
    declara::core::EmitterState toggleEvent_;
    BmiCalculatorComponent bmiCalculator_;
  };

  class MainRightPanelComponent
  {
  public:
    struct FruitPopupLabel
    {
      typedef loka::core::String Result;
      loka::core::String operator()(const loka::core::String &value) const { return value; }
    };

    explicit MainRightPanelComponent(MainNode *owner);
    void attachNode(declara::core::scene::NodeComposition &c);
    void composeNode(declara::core::scene::NodeComposition &c);

  private:
    void handleFruitChanged();

    MainNode *owner_;
    bool initialized_;
    declara::core::scene::BoundState<int> fruitIndex_;
    declara::core::scene::BoundState<loka::core::String> fruitMessage_;
    loka::Vector<loka::core::String> fruits_;
    declara::core::EmitterState fruitChangedEvent_;
  };

  class MainNode : public declara::core::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : declara::core::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
          left_(this),
          right_(this)
    {
    }

    template <class NodeT>
    void bindForUiComponent(declara::core::EmitterState &emitter, NodeT *node, void (NodeT::*method)())
    {
      this->bindForUi(emitter, node, method);
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *ctx = this->attachedContext();
      return ctx ? ctx->window() : 0;
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c);

    MainLeftPanelComponent &left() { return left_; }
    MainRightPanelComponent &right() { return right_; }

  private:
    MainLeftPanelComponent left_;
    MainRightPanelComponent right_;
  };

  inline MainLeftPanelComponent::MainLeftPanelComponent(MainNode *owner)
      : owner_(owner), initialized_(false), message_(), toggleEvent_(), bmiCalculator_()
  {
  }

  inline void MainLeftPanelComponent::attachNode(declara::core::scene::NodeComposition &c)
  {
    if (initialized_)
    {
      return;
    }
    c.declareStates()
        .state(message_, loka::core::String::Literal("Hello, Loka!"));
    if (owner_)
    {
      owner_->bindForUiComponent(toggleEvent_, this, &MainLeftPanelComponent::toggleMessage);
    }
    initialized_ = true;
  }

  inline void MainLeftPanelComponent::composeNode(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    if (!owner_)
    {
      return;
    }
    c.declare(
        VStack()
        << Text("Loka Sample")
        << Text(message_)
        << Button("Add +", &toggleEvent_)
        << declara::core::scene::LC(bmiCalculator_));
  }

  inline void MainLeftPanelComponent::toggleMessage()
  {
    if (!message_.isValid())
    {
      return;
    }
    if (!owner_)
    {
      return;
    }
    ::Window *window = owner_->windowOrNull();
    loka::core::String next = message_.get() + " +Loka";
    message_.set(next, true);

    if (!window)
    {
      return;
    }
    {
      StateTrackerGuard _(window->getTracker());
      const loka::core::String title = window->titleState().get();

      if (title.equals(loka::core::String::Literal("LokaSample")))
      {
        window->titleState().set(loka::core::String::Literal("LokaSample*"));
      }
      else
      {
        window->titleState().set(loka::core::String::Literal("LokaSample"));
      }
    }
  }

  inline MainRightPanelComponent::MainRightPanelComponent(MainNode *owner)
      : owner_(owner),
        initialized_(false),
        fruitIndex_(),
        fruitMessage_(),
        fruits_(),
        fruitChangedEvent_()
  {
    fruits_.assign(kFruitItems, kFruitItemCount);
  }

  inline void MainRightPanelComponent::attachNode(declara::core::scene::NodeComposition &c)
  {
    if (initialized_)
    {
      return;
    }
    c.declareStates()
        .state(fruitIndex_, 0)
        .state(fruitMessage_, loka::core::String::Literal("You chose Apple."));
    if (owner_)
    {
      owner_->bindForUiComponent(fruitChangedEvent_, this, &MainRightPanelComponent::handleFruitChanged);
    }
    initialized_ = true;
  }

  inline void MainRightPanelComponent::composeNode(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    if (!owner_)
    {
      return;
    }
    c.declare(
        VStack()
        << Text("Fruit Picker")
        << PopupMenu(fruits_.map<loka::core::String>(FruitPopupLabel()))
               .selectedIndex(fruitIndex_)
               .onChange(&fruitChangedEvent_)
        << Text(fruitMessage_));
  }

  inline void MainRightPanelComponent::handleFruitChanged()
  {
    if (!fruitIndex_.isValid() || !fruitMessage_.isValid() || fruits_.empty())
    {
      return;
    }
    int index = fruitIndex_.get();
    if (index < 0 || static_cast<std::size_t>(index) >= fruits_.size())
    {
      index = 0;
    }
    loka::core::String next = loka::core::String::Literal("You chose ") + fruits_[static_cast<std::size_t>(index)] + ".";
    fruitMessage_.set(next, true);
  }

  inline void MainNode::composeNode(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    c.declare(
        HStack()
        << declara::core::scene::LC(left_)
        << declara::core::scene::LC(right_));
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_NODE_HPP
