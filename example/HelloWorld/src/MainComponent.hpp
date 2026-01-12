#ifndef LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
#define LOKA_HELLOWORLD_MAIN_COMPONENT_HPP

#include "core/util/StateTrackerGuard.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/node/StaticComposition.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/PopupMenu.hpp"
#include "ToolboxControlSlots.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Vector.hpp"
#include "loka/platform/StringUTF8.hpp"
#include <string>
#include <cstdio>
#include <cstdlib>

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
      {
        declara::core::scene::NodeComposition::StateBatch states = c.declareStates();
        states.state(message_, loka::core::String::Literal("Hello, Loka!"))
            .state(fruitIndex_, 0)
            .state(fruitMessage_, loka::core::String::Literal("Apple."))
            .state(heightInput_, loka::core::String::Literal("170.0"))
            .state(weightInput_, loka::core::String::Literal("60.0"))
            .state(bmiResult_, loka::core::String::Literal("BMI: --"));
#if defined(LOKA_RETRO68)
        states.state(profileResult_, loka::core::String(gProfileResultString));
#else
        states.state(profileResult_, loka::core::String::Literal(""));
#endif
      } // StateBatch destructor runs here, states become valid
      this->bindForUi(toggleEvent_, this, &MainNode::toggleMessage);
      this->bindForUi(fruitChangedEvent_, this, &MainNode::handleFruitChanged);
      heightInput_.bind(&MainNode::BmiInputChangedThunk, this, false);
      weightInput_.bind(&MainNode::BmiInputChangedThunk, this, false);
      updateBmi();
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
      loka::core::String next = loka::core::String::Literal(" ") + this->fruits_[static_cast<std::size_t>(index)] + ".";
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

    void updateBmi()
    {
      const AttachedContext *ctx = this->attachedContext();
      if (!ctx) return;
      double heightCm = parseBmiDouble(heightInput_.get());
      double weightKg = parseBmiDouble(weightInput_.get());
      loka::core::String label = loka::core::String::Literal("BMI: --");
      if (heightCm > 0.0 && weightKg > 0.0)
      {
        double heightM = heightCm / 100.0;
        if (heightM > 0.0)
        {
          double bmi = weightKg / (heightM * heightM);
          char buf[64];
          std::snprintf(buf, sizeof(buf), "BMI: %.2f", bmi);
          label = loka::core::String(std::string(buf));
        }
      }
      StateTrackerGuard _(ctx->boundary()->tracker());
      bmiResult_.set(label, true);
    }

    static double parseBmiDouble(const loka::core::String &value)
    {
      std::string utf8;
      if (!loka::platform::CollectUtf8(value, utf8)) return 0.0;
      char *endPtr = 0;
      double parsed = std::strtod(utf8.c_str(), &endPtr);
      return (endPtr == utf8.c_str()) ? 0.0 : parsed;
    }

    static void BmiInputChangedThunk(void *userData)
    {
      MainNode *self = static_cast<MainNode *>(userData);
      if (self) self->updateBmi();
    }

    declara::core::scene::BoundState<loka::core::String> message_;
    declara::core::scene::BoundState<int> fruitIndex_;
    declara::core::scene::BoundState<loka::core::String> fruitMessage_;
    declara::core::scene::BoundState<loka::core::String> profileResult_;
    declara::core::scene::BoundState<loka::core::String> heightInput_;
    declara::core::scene::BoundState<loka::core::String> weightInput_;
    declara::core::scene::BoundState<loka::core::String> bmiResult_;
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
            << Text("BMI Calculator")
            << Text("Height (cm)")
            << EditText(this->props.owner->heightInput_).toolboxControl(kToolboxControlHeightInput)
            << Text("Weight (kg)")
            << EditText(this->props.owner->weightInput_).toolboxControl(kToolboxControlWeightInput)
            << Text(this->props.owner->bmiResult_));
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
