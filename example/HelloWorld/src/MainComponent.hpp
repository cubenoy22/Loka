#ifndef LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
#define LOKA_HELLOWORLD_MAIN_COMPONENT_HPP

#include "core/util/StateTrackerGuard.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/node/StaticComposition.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/PopupMenu.hpp"
#include "BmiCalculatorComponent.hpp"
#include "ToolboxControlSlots.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include <string>

#if defined(LOKA_RETRO68)
extern std::string gProfileResultString;
#endif

namespace helloworld
{
  class MainNode;
  typedef declara::core::scene::StaticCompositionPropsFor<MainNode> MainProps;

  struct MainPanelTypeTag
  {
  };

  class MainPanelNode;

  struct MainPanelProps : public declara::core::scene::NodePropsBase<MainPanelProps>
  {
    typedef MainPanelTypeTag TypeTag;
    typedef MainPanelNode NodeType;
    MainNode *owner;
    bool leftSide;
    MainPanelProps() : owner(0), leftSide(true) {}
    MainPanelProps(MainNode *o, bool left) : owner(o), leftSide(left) {}
    bool operator<(const declara::core::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != propsTypeId())
        return false;
      const MainPanelProps &other = static_cast<const MainPanelProps &>(rhs);
      return owner < other.owner;
    }
  };

  inline declara::core::scene::NodeDefinition<MainPanelProps, MainPanelNode> MainPanel(MainNode *owner, bool leftSide);

  class MainNode : public declara::core::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : declara::core::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
          message_(),
          fruitIndex_(),
          fruitMessage_(),
          toggleEvent_()
    {
      this->fruits_.push_back(loka::core::String::Literal("Apple"));
      this->fruits_.push_back(loka::core::String::Literal("Banana"));
      this->fruits_.push_back(loka::core::String::Literal("Cherry"));
      this->fruits_.push_back(loka::core::String::Literal("Grape"));
      // State is initialized in attachNode via NodeComposition::declareStates.
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      declara::core::scene::NodeComposition::StateBatch states = c.declareStates();
      states.state(message_, loka::core::String::Literal("Hello, Loka!"))
          .state(fruitIndex_, 0)
          .state(fruitMessage_, loka::core::String::Literal("You chose Apple."));
#if defined(LOKA_RETRO68)
      states.state(profileResult_, loka::core::String(gProfileResultString));
#else
      states.state(profileResult_, loka::core::String::Literal(""));
#endif
      this->bindForUi(toggleEvent_, this, &MainNode::toggleMessage);
      this->bindForUi(fruitChangedEvent_, this, &MainNode::handleFruitChanged);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c);

  private:
    friend class MainPanelNode;
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
    declara::core::scene::BoundState<loka::core::String> profileResult_;
    loka::Vector<loka::core::String> fruits_;
    EmitterState toggleEvent_;
    EmitterState fruitChangedEvent_;
  };

  class MainPanelNode : public declara::core::scene::StaticCompositionBoundaryNodeBase<MainPanelProps>
  {
  public:
    MainPanelNode(const MainPanelProps &p)
        : declara::core::scene::StaticCompositionBoundaryNodeBase<MainPanelProps>(p) {}

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      if (!this->props.owner)
      {
        return;
      }
      if (this->props.leftSide)
      {
        c.declare(
            VStack()
            << Text("Loka Sample")
            << Text(this->props.owner->message_)
            << Button("Add +", &this->props.owner->toggleEvent_).toolboxControl(kToolboxControlAddButton)
            << BmiCalculator());
        return;
      }
      c.declare(
          VStack()
          << Text("Fruit Picker")
          << PopupMenu(&this->props.owner->fruits_).selectedIndex(this->props.owner->fruitIndex_).onChange(&this->props.owner->fruitChangedEvent_)
          << Text(this->props.owner->fruitMessage_));
    }
  };

  inline declara::core::scene::NodeDefinition<MainPanelProps, MainPanelNode> MainPanel(MainNode *owner, bool leftSide)
  {
    return declara::core::scene::NodeDefinition<MainPanelProps, MainPanelNode>(MainPanelProps(owner, leftSide));
  }

  inline void MainNode::composeNode(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    c.declare(
        HStack()
        << MainPanel(this, true)
        << MainPanel(this, false));
  }

  inline declara::core::scene::NodeDefinition<MainProps, MainNode> Main()
  {
    return declara::core::scene::NodeDefinition<MainProps, MainNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
