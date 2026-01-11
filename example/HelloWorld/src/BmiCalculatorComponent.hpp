#ifndef LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
#define LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP

#include <string>
#include <cstdio>
#include <cstdlib>
#include "core2/scene/BoundState.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "loka/core/String.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "core2/scene/node/Group.hpp"
#include "app/EditText.hpp"
#include "app/Text.hpp"
#include "app/RowColumn.hpp"
#include "ToolboxControlSlots.hpp"

namespace helloworld
{
  class BmiCalculatorNode;
  typedef declara::core::scene::GroupPropsFor<BmiCalculatorNode> BmiCalculatorProps;

  class BmiCalculatorNode : public declara::core::scene::GroupNodeBase<BmiCalculatorProps>
  {
  public:
    BmiCalculatorProps props;
    BmiCalculatorNode(const BmiCalculatorProps &p)
        : declara::core::scene::GroupNodeBase<BmiCalculatorProps>(BmiCalculatorProps(p)),
          props(p),
          heightInput_(),
          weightInput_(),
          bmiResult_()
    {
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      c.declareStates()
          .state(heightInput_, loka::core::String::Literal("170.0"))
          .state(weightInput_, loka::core::String::Literal("60.0"))
          .state(bmiResult_, loka::core::String::Literal("BMI: --"));
      heightInput_.bind(&BmiCalculatorNode::InputChangedThunk, this, false);
      weightInput_.bind(&BmiCalculatorNode::InputChangedThunk, this, false);
      updateBmi();
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(
          VStack()
          << Text("BMI Calculator")
          << Text("Height (cm)")
          << EditText(heightInput_).toolboxControl(kToolboxControlHeightInput)
          << Text("Weight (kg)")
          << EditText(weightInput_).toolboxControl(kToolboxControlWeightInput)
          << Text(bmiResult_));
    }

  private:
    static double parseDouble(const loka::core::String &value)
    {
      std::string utf8;
      if (!loka::platform::CollectUtf8(value, utf8))
      {
        return 0.0;
      }
      const char *start = utf8.c_str();
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
      BmiCalculatorNode *self = static_cast<BmiCalculatorNode *>(userData);
      if (self)
      {
        self->updateBmi();
      }
    }

    void updateBmi()
    {
      const AttachedContext *ctx = this->attachedContext();
      if (!ctx)
      {
        return;
      }
      double heightCm = parseDouble(heightInput_.get());
      double weightKg = parseDouble(weightInput_.get());
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

    declara::core::scene::BoundState<loka::core::String> heightInput_;
    declara::core::scene::BoundState<loka::core::String> weightInput_;
    declara::core::scene::BoundState<loka::core::String> bmiResult_;
  };

  inline declara::core::scene::NodeDefinition<BmiCalculatorProps, BmiCalculatorNode> BmiCalculator()
  {
    return declara::core::scene::NodeDefinition<BmiCalculatorProps, BmiCalculatorNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
