#ifndef DECLARA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
#define DECLARA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP

#include <string>
#include "core/State.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app/EditText.hpp"
#include "app/Text.hpp"
#include "app/RowColumn.hpp"

namespace helloworld
{
  struct BmiCalculatorProps
  {
    State<std::string> *heightInput;
    State<std::string> *weightInput;
    State<std::string> *bmiResult;
    BmiCalculatorProps()
        : heightInput(0), weightInput(0), bmiResult(0) {}
    BmiCalculatorProps(State<std::string> *h,
                       State<std::string> *w,
                       State<std::string> *b)
        : heightInput(h), weightInput(w), bmiResult(b) {}
  };

  inline declara::core::scene::NodeDefinitionBase *BmiCalculator(declara::core::scene::NodeComposition &c, const BmiCalculatorProps &props)
  {
    using namespace declara::app;
    VStack layout;
    layout << Text(TextProps().setText("BMI Calculator"))
           << Text(TextProps().setText("Height (cm)"))
           << EditText(EditTextProps().setText(props.heightInput))
           << Text(TextProps().setText("Weight (kg)"))
           << EditText(EditTextProps().setText(props.weightInput))
           << Text(TextProps().setText(props.bmiResult));
    return c.group(layout);
  }
} // namespace helloworld

#endif // DECLARA_HELLOWORLD_BMI_CALCULATOR_COMPONENT_HPP
