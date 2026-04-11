#ifndef LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
#define LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP

#include "loka/core/String.hpp"
#include "app/EditText.hpp"
#include "app/Text.hpp"
#include "app/RowColumn.hpp"
#include "loka/core/State.hpp"

namespace helloworld
{
  struct BmiCalculatorProps
  {
    BmiCalculatorProps(loka::core::State<loka::core::String> *heightValue,
                       loka::core::State<loka::core::String> *weightValue,
                       loka::core::State<loka::core::String> *resultValue)
        : heightInput(heightValue),
          weightInput(weightValue),
          bmiResult(resultValue) {}

    loka::core::State<loka::core::String> *heightInput;
    loka::core::State<loka::core::String> *weightInput;
    loka::core::State<loka::core::String> *bmiResult;
  };

  inline loka::app::VStack BmiCalculator(const BmiCalculatorProps &props)
  {
    using namespace loka::app;
    return VStack()
           << Text("BMI Calculator")
           << Text("Height (cm)")
           << EditText(props.heightInput)
           << Text("Weight (kg)")
           << EditText(props.weightInput)
           << Text(props.bmiResult);
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
