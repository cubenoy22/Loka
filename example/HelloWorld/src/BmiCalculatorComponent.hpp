#ifndef LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
#define LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP

#include "loka/core/String.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "loka/core/State.hpp"

namespace helloworld
{
  inline loka::app::VStack BmiCalculator(loka::core::State<loka::core::String> *heightInput,
                                         loka::core::State<loka::core::String> *weightInput,
                                         loka::core::State<loka::core::String> *bmiResult)
  {
    using namespace loka::app;
    return VStack()
           << Text("BMI Calculator")
           << Text("Height (cm)")
           << EditText(heightInput)
           << Text("Weight (kg)")
           << EditText(weightInput)
           << Text(bmiResult);
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
