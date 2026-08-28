#ifndef LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
#define LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP

#include "core/String.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "core/State.hpp"

namespace helloworld
{
  using loka::app::scene::NodeState;
  using loka::core::State;
  using loka::core::String;

  inline loka::app::F BmiCalculator(const NodeState<String> &heightInput,
                                    const NodeState<String> &weightInput,
                                    State<String> *bmiResult)
  {
    using namespace loka::app;
    return F().TEST_ID("HelloWorld.Bmi") //
           << Text("BMI Calculator") //
           << Text("Height (cm)")    //
           << EditText(heightInput)   //
           << Text("Weight (kg)")    //
           << EditText(weightInput)   //
           << Text(bmiResult);
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
