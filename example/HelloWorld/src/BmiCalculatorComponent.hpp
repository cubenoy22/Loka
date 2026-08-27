#ifndef LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
#define LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP

#include "core/String.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "core/State.hpp"

namespace helloworld
{
  inline loka::app::F BmiCalculator(const loka::app::scene::NodeState<loka::core::String> &heightInput,
                                    const loka::app::scene::NodeState<loka::core::String> &weightInput,
                                    loka::core::State<loka::core::String> *bmiResult)
  {
    using namespace loka::app;
    return F()                       //
           << Text("BMI Calculator") //
           << Text("Height (cm)")    //
           << EditText(heightInput).TEST_ID("HelloWorld.Bmi.HeightInput") //
           << Text("Weight (kg)")    //
           << EditText(weightInput).TEST_ID("HelloWorld.Bmi.WeightInput") //
           << Text(bmiResult).TEST_ID("HelloWorld.Bmi.Result");
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
