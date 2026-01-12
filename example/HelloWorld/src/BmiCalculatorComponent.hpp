#ifndef LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
#define LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP

#include <string>
#include <cstdio>
#include <cstdlib>
#include "core2/scene/BoundState.hpp"
#include "core2/scene/Component.hpp"
#include "loka/core/String.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "app/EditText.hpp"
#include "app/Text.hpp"
#include "app/RowColumn.hpp"
#include "ToolboxControlSlots.hpp"

namespace helloworld
{
  class BmiCalculatorComponent
  {
  public:
    BmiCalculatorComponent()
        : initialized_(false),
          heightInput_(),
          weightInput_(),
          bmiResult_()
    {}

    void composeInto(declara::core::scene::NodeComposition &c,
                     declara::core::scene::INestableDefinition &parent)
    {
      if (!initialized_)
      {
        c.declareStates()
            .state(heightInput_, loka::core::String::Literal("170.0"))
            .state(weightInput_, loka::core::String::Literal("60.0"))
            .state(bmiResult_, loka::core::String::Literal("BMI: --"));
        heightInput_.bind(&BmiCalculatorComponent::InputChangedThunk, this, false);
        weightInput_.bind(&BmiCalculatorComponent::InputChangedThunk, this, false);
        updateBmi();
        initialized_ = true;
      }
      using namespace declara::app;
      parent
          << Text("BMI Calculator")
          << Text("Height (cm)")
          << EditText(heightInput_).toolboxControl(kToolboxControlHeightInput)
          << Text("Weight (kg)")
          << EditText(weightInput_).toolboxControl(kToolboxControlWeightInput)
          << Text(bmiResult_);
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
      BmiCalculatorComponent *self = static_cast<BmiCalculatorComponent *>(userData);
      if (self)
      {
        self->updateBmi();
      }
    }

    void updateBmi()
    {
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
      bmiResult_.set(label, true);
    }

    bool initialized_;
    declara::core::scene::BoundState<loka::core::String> heightInput_;
    declara::core::scene::BoundState<loka::core::String> weightInput_;
    declara::core::scene::BoundState<loka::core::String> bmiResult_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
