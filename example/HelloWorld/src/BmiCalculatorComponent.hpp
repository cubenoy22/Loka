#ifndef LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
#define LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP

#include <string>
#include <cstdio>
#include <cstdlib>
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "core2/scene/node/Group.hpp"
#include "app/EditText.hpp"
#include "app/Text.hpp"
#include "app/RowColumn.hpp"

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
          heightInput_(0),
          weightInput_(0),
          bmiResult_(0)
    {
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      heightInput_ = &c.useState<std::string>("170.0");
      weightInput_ = &c.useState<std::string>("60.0");
      bmiResult_ = &c.useState<std::string>("BMI: --");
      heightInput_->bind(&BmiCalculatorNode::InputChangedThunk, this, false);
      weightInput_->bind(&BmiCalculatorNode::InputChangedThunk, this, false);
      updateBmi();
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(
          VStack()
          << Text("BMI Calculator")
          << Text("Height (cm)")
          << EditText(heightInput_)
          << Text("Weight (kg)")
          << EditText(weightInput_)
          << Text(bmiResult_));
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
      BmiCalculatorNode *self = static_cast<BmiCalculatorNode *>(userData);
      if (self)
      {
        self->updateBmi();
      }
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
      StateTrackerGuard _(this->boundary()->tracker());
      bmiResult_->set(label, true);
    }

    MutableState<std::string> *heightInput_;
    MutableState<std::string> *weightInput_;
    MutableState<std::string> *bmiResult_;
  };

  inline declara::core::scene::NodeDefinition<BmiCalculatorProps, BmiCalculatorNode> BmiCalculator()
  {
    return declara::core::scene::NodeDefinition<BmiCalculatorProps, BmiCalculatorNode>();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
