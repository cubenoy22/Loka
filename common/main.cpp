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

// --- BMICalcScene: BMI計算シーンのSceneNodeベース実装 ---
class BMICalcScene : public Scene
{
public:
  BMICalcScene()
      : Scene(new SceneHost()),
        heightStr("170.0"),
        weightStr("60.0"),
        height({&heightStr}, &strToDoubleThunk),
        weight({&weightStr}, &strToDoubleThunk),
        bmi({&heightStr, &weightStr}, &BMIMultiArgCalcBMIThunk),
        tracker({&heightStr, &weightStr, &height, &weight, &bmi}) {}

  // C++98: DerivedStateの関数ポインタ用staticラッパ
  static double strToDoubleThunk()
  {
    // 実際の値は呼び出し元で取得するため未使用
    return 0.0;
  }
  static double BMIMultiArgCalcBMIThunk()
  {
    return 0.0;
  }
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

  void compose(SceneNodeGroup &group)
  {
    group.add(new SceneNodeText("身長(cm)を入力してください"));
    group.add(new SceneNodeTextInput(&heightStr));
    group.add(new SceneNodeText("体重(kg)を入力してください"));
    group.add(new SceneNodeTextInput(&weightStr));
    group.add(new SceneNodeText("BMI: " + doubleToStr(bmi.get())));
    // ...ボタン等追加可...
  }

  MutableState<std::string> heightStr;
  MutableState<std::string> weightStr;
  DerivedState<double> height;
  DerivedState<double> weight;
  DerivedState<double> bmi;
  PushStateTracker tracker;
};
