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
      PROFILE_SECTION("bmiCompose");
      if (!this->initialized_)
      {
        PROFILE_SECTION("bmiInit");
        c.declareStates()
            .state(this->heightInput_, loka::core::String::Literal("170.0"))
            .state(this->weightInput_, loka::core::String::Literal("60.0"))
            .state(this->bmiResult_, loka::core::String::Literal("BMI: --"));
        this->heightInput_.bind(&BmiChangedThunk, this, false);
        this->weightInput_.bind(&BmiChangedThunk, this, false);
        updateBmi();
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
    static void BmiChangedThunk(void *userData)
    {
      BmiCalculatorComponent *self = static_cast<BmiCalculatorComponent *>(userData);
      if (self)
      {
        self->updateBmi();
      }
    }

    double parseDouble(const loka::core::String &value) const
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

    void updateBmi()
    {
      if (!this->heightInput_.isValid() || !this->weightInput_.isValid() || !this->bmiResult_.isValid())
      {
        return;
      }
      double heightCm = parseDouble(this->heightInput_.get());
      double weightKg = parseDouble(this->weightInput_.get());
      if (heightCm <= 0.0 || weightKg <= 0.0)
      {
        this->bmiResult_.set(loka::core::String::Literal("BMI: --"), true);
        return;
      }
      double heightM = heightCm / 100.0;
      if (heightM <= 0.0)
      {
        this->bmiResult_.set(loka::core::String::Literal("BMI: --"), true);
        return;
      }
      double bmi = weightKg / (heightM * heightM);
      if (bmi <= 0.0)
      {
        this->bmiResult_.set(loka::core::String::Literal("BMI: --"), true);
        return;
      }
      char buf[64];
      std::snprintf(buf, sizeof(buf), "BMI: %.2f", bmi);
      this->bmiResult_.set(loka::core::String(std::string(buf)), true);
    }

    bool initialized_;
    declara::core::scene::BoundState<loka::core::String> heightInput_;
    declara::core::scene::BoundState<loka::core::String> weightInput_;
    declara::core::scene::BoundState<loka::core::String> bmiResult_;
  };

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
