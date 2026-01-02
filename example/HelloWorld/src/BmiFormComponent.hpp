#ifndef DECLARA_HELLOWORLD_BMI_FORM_COMPONENT_HPP
#define DECLARA_HELLOWORLD_BMI_FORM_COMPONENT_HPP

#include <string>
#include <cstdio>
#include <cstdlib>
#include "core/State.hpp"
#include "core/util/AutoTransactionGuard.hpp"
#include "core2/scene/node/StaticComposition.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "BmiCalculatorComponent.hpp"

namespace helloworld
{
  struct BmiFormTypeTag
  {
  };

  struct BmiFormProps : public declara::core::scene::StaticCompositionProps
  {
    typedef BmiFormTypeTag TypeTag;
  };

  class BmiFormNode : public declara::core::scene::StaticCompositionNode
  {
  public:
    typedef BmiFormTypeTag TypeTag;
    BmiFormProps props;
    BmiFormNode(const BmiFormProps &p)
        : declara::core::scene::StaticCompositionNode(BmiFormProps(p)),
          props(p),
          heightInput_(0),
          weightInput_(0),
          bmiResult_(0),
          message_(0),
          toggleEvent_()
    {
      // State is initialized lazily in composeNode via NodeComposition::useState.
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      if (!heightInput_)
      {
        heightInput_ = &c.useState<std::string>("170.0");
        weightInput_ = &c.useState<std::string>("60.0");
        bmiResult_ = &c.useState<std::string>("BMI: --");
        message_ = &c.useState<std::string>("Hello, Declara!");
        heightInput_->bind(&BmiFormNode::InputChangedThunk, this, false);
        weightInput_->bind(&BmiFormNode::InputChangedThunk, this, false);
        toggleEvent_.bind(&BmiFormNode::ToggleMessageThunk, this, false);
        updateBmi();
      }
      using namespace declara::app;
      c.declare(
          VStack()
          << Text(TextProps().setText("Loka Sample"))
          << Text(TextProps().setText(message_))
          << Button(ButtonProps().setText("Toggle Message").setOnClick(&toggleEvent_))
          << BmiCalculator(c, BmiCalculatorProps(heightInput_, weightInput_, bmiResult_)));
    }

  private:
    static double parseDouble(const std::string &value)
    {
      const char *start = value.c_str();
      char *endPtr = 0;
      double parsed = std::strtod(start, &endPtr);
      if (endPtr == start)
      {
        return 0.0;
      }
      return parsed;
    }

    static void InputChangedThunk(void *userData)
    {
      BmiFormNode *self = static_cast<BmiFormNode *>(userData);
      if (self)
      {
        self->updateBmi();
      }
    }

    static void ToggleMessageThunk(void *userData)
    {
      BmiFormNode *self = static_cast<BmiFormNode *>(userData);
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
      std::string next = "Hello, Declara!";
      if (message_->get() == "Hello, Declara!")
      {
        next = "Context says hi!";
      }
      AutoTransactionGuard guard(message_->currentTracker);
      message_->set(next, true);
    }

    void updateBmi()
    {
      double heightCm = parseDouble(heightInput_->get());
      double weightKg = parseDouble(weightInput_->get());
      std::string label("BMI: --");
      if (heightCm > 0.0 && weightKg > 0.0)
      {
        double heightM = heightCm / 100.0;
        if (heightM > 0.0)
        {
          double bmi = weightKg / (heightM * heightM);
          char buf[64];
          std::sprintf(buf, "BMI: %.2f", bmi);
          label = buf;
        }
      }
      AutoTransactionGuard guard(bmiResult_->currentTracker);
      bmiResult_->set(label, true);
    }

    MutableState<std::string> *heightInput_;
    MutableState<std::string> *weightInput_;
    MutableState<std::string> *bmiResult_;
    MutableState<std::string> *message_;
    EmitterState toggleEvent_;
  };

  struct BmiFormDefinition : public declara::core::scene::BoundaryDefinition<BmiFormProps, BmiFormNode>
  {
    BmiFormDefinition() : BoundaryDefinition<BmiFormProps, BmiFormNode>() {}
  };

  inline BmiFormDefinition BmiForm()
  {
    return BmiFormDefinition();
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_BMI_FORM_COMPONENT_HPP
