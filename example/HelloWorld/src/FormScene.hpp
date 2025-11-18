#ifndef DECLARA_FORM_SCENE_HPP
#define DECLARA_FORM_SCENE_HPP

#include "core/State.hpp"
#include <string>
#include <cstdio>
#include <cstdlib>
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app2/EditText.hpp"
#include "app2/Fragment.hpp"
#include "app2/RowColumn.hpp"
#include "app2/Text.hpp"

class FormScene : public declara::core::scene::Scene
{
public:
  FormScene()
      : Scene(),
        heightInput_("170.0"),
        weightInput_("60.0"),
        bmiResult_("BMI: --")
  {
    heightInput_.bind(&FormScene::InputChangedThunk, this, false);
    weightInput_.bind(&FormScene::InputChangedThunk, this, false);
    updateBmi();
  }

  virtual void compose(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;

    c.declare(
        VStack()
        << c.group(
               F()
               << Text(TextProps().setText("BMI Calculator"))
               << Text(TextProps().setText("Height (cm)"))
               << EditText(EditTextProps().setText(&heightInput_))
               << Text(TextProps().setText("Weight (kg)"))
               << EditText(EditTextProps().setText(&weightInput_))
               << Text(TextProps().setText(&bmiResult_))));
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
};

#endif // DECLARA_FORM_SCENE_HPP
