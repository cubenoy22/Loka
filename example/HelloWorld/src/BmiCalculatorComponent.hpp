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
#include "loka/dsl/StateStream.hpp"
#include "ToolboxControlSlots.hpp"
#include "app/Fragment.hpp"

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
      if (!this->initialized_)
      {
        c.declareStates()
            .state(this->heightInput_, loka::core::String::Literal("170.0"))
            .state(this->weightInput_, loka::core::String::Literal("60.0"))
            .state(this->bmiResult_, loka::core::String::Literal("BMI: --"));
        this->heightInput_.stream()
            .map(ToDouble())
            .combine(this->weightInput_.stream().map(ToDouble()), ComputeBmi())
            .map(FormatBmi())
            .set(this->bmiResult_, true);
        this->initialized_ = true;
      }
      using namespace declara::app;
      parent
          << Text("BMI Calculator")
          << Text("Height (cm)")
          << EditText(this->heightInput_).toolboxControl(kToolboxControlHeightInput)
          << Text("Weight (kg)")
          << EditText(this->weightInput_).toolboxControl(kToolboxControlWeightInput)
          << Text(this->bmiResult_);
    }

  private:
    struct ToDouble
    {
      typedef double Result;

      double operator()(const loka::core::String &value) const
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
    };

    struct ComputeBmi
    {
      typedef double Result;

      double operator()(double heightCm, double weightKg) const
      {
        if (heightCm <= 0.0 || weightKg <= 0.0)
        {
          return 0.0;
        }
        double heightM = heightCm / 100.0;
        if (heightM <= 0.0)
        {
          return 0.0;
        }
        return weightKg / (heightM * heightM);
      }
    };

    struct FormatBmi
    {
      typedef loka::core::String Result;

      loka::core::String operator()(double bmi) const
      {
        if (bmi <= 0.0)
        {
          return loka::core::String::Literal("BMI: --");
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "BMI: %.2f", bmi);
        return loka::core::String(std::string(buf));
      }
    };

    bool initialized_;
    declara::core::scene::BoundState<loka::core::String> heightInput_;
    declara::core::scene::BoundState<loka::core::String> weightInput_;
    declara::core::scene::BoundState<loka::core::String> bmiResult_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
