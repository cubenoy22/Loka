#include "core/StateTracker.hpp"
#include "core/Scene.hpp"
#include "app/Button.hpp"
#include "core/PlatformContext.hpp"
#include "core/App.hpp"
#include "core/Window.hpp"
#include <string>
#include <iostream>
#include <cassert>

// C++98互換: 2倍値計算用のグローバル関数
static int doubleFn(const int &v) { return v * 2; }

class FormScene : public Scene
{
public:
  FormScene()
      : Scene(new SceneHost()),
        name(""),
        isValid({&name}, [&]()
                { return name.get().length() >= 3; }),
        tracker({&name, &isValid}) {}
  static bool evaluateLength(const std::string &s) { return s.length() >= 3; }
  static void onSendClick() {}
  void compose(SceneBuilder &builder)
  {
    builder
        .Text("名前を入力してください")
        .TextInput(&name)
        .Button(
            ButtonOptions()
                .setLabel("送信")
                .setEnabled(&isValid)
                .setOnClick(&FormScene::onSendClick));
  }
  MutableState<std::string> name;
  DerivedState<bool> isValid;
  PushStateTracker tracker;
};

// --- BMICalcScene: BMI計算シーンの例 ---
class BMICalcScene : public Scene
{
public:
  BMICalcScene()
      : Scene(new SceneHost()),
        heightStr("170.0"),
        weightStr("60.0"),
        height({&heightStr}, [&]()
               { return strToDouble(heightStr.get()); }),
        weight({&weightStr}, [&]()
               { return strToDouble(weightStr.get()); }),
        bmi({&heightStr, &weightStr}, [&]()
            { return BMIMultiArgCalcBMI(heightStr.get(), weightStr.get()); }),
        tracker({&heightStr, &weightStr, &height, &weight, &bmi}) {}

  static double strToDouble(const std::string &s) { return atof(s.c_str()); }
  static std::string doubleToStr(const double &v)
  {
    char buf[32];
    sprintf(buf, "%.2f", v);
    return std::string(buf);
  }
  static double BMIMultiArgCalcBMI(const std::string &hStr, const std::string &wStr)
  {
    double h = strToDouble(hStr);
    double w = strToDouble(wStr);
    if (h <= 0)
      return 0.0;
    double m = h / 100.0;
    return w / (m * m);
  }
  void compose(SceneBuilder &b)
  {
    b.Text("身長(cm)を入力してください");
    b.TextInput(&heightStr);
    b.Text("体重(kg)を入力してください");
    b.TextInput(&weightStr);
    b.Text("BMI: " + doubleToStr(bmi.get()));
    // ...ボタン等...
  }
  MutableState<std::string> heightStr;
  MutableState<std::string> weightStr;
  DerivedState<double> height;
  DerivedState<double> weight;
  DerivedState<double> bmi;
  PushStateTracker tracker;
};
