#ifndef DECLARA_FORM_SCENE_HPP
#define DECLARA_FORM_SCENE_HPP

#include "core/State.hpp"
#include <string>
#include <cstdio>
#include <cstdlib>
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/Fragment.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "BmiCalculatorComponent.hpp"
#include "ErrorTestComponent.hpp"

class FormScene : public declara::core::scene::Scene
{
public:
  FormScene()
      : Scene(),
        heightInput_("170.0"),
        weightInput_("60.0"),
        bmiResult_("BMI: --"),
        showError_(false),
        errorTitle_(""),
        errorBody_("")
  {
    heightInput_.bind(&FormScene::InputChangedThunk, this, false);
    weightInput_.bind(&FormScene::InputChangedThunk, this, false);
    updateBmi();
  }

  virtual void compose(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    using namespace helloworld;

    c.declare(
        HStack()
        << c.group(
               VStack() << c.group(
                   F()
                   << Text(TextProps().setText("Loka Sample"))
                   << Button(ButtonProps().setText("Show Error").setOnClick(&onShowError_))))
        << BmiCalculator(c, BmiCalculatorProps(&heightInput_, &weightInput_, &bmiResult_)));

    ErrorTestSetup(
        ErrorTestProps()
            .setTitle(&errorTitle_)
            .setBody(&errorBody_)
            .setShow(&showError_)
            .setTrigger(&onShowError_));
  }

private:
  static double parseDouble(const std::string &value)
  {
    const char *start = value.c_str();
    char *endPtr = NULL;
    double parsed = std::strtod(start, &endPtr);
    if (endPtr == start)
    {
      return 0.0;
    }
    return parsed;
  }

  static void InputChangedThunk(void *userData)
  {
    FormScene *self = static_cast<FormScene *>(userData);
    if (self)
    {
      self->updateBmi();
    }
  }

  void updateBmi()
  {
    double heightCm = parseDouble(heightInput_.get());
    double weightKg = parseDouble(weightInput_.get());
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
    bmiResult_.set(label, true);
  }

  MutableState<std::string> heightInput_;
  MutableState<std::string> weightInput_;
  MutableState<std::string> bmiResult_;
  MutableState<bool> showError_;
  MutableState<std::string> errorTitle_;
  MutableState<std::string> errorBody_;
  EmitterState onShowError_;

  // Error handling is delegated to ErrorTestSetup
};

#endif // DECLARA_FORM_SCENE_HPP
