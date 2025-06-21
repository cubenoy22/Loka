#include "core/StateTracker.hpp"
#include "core/Scene.hpp"
#include "app/Button.hpp"
#include "app/TextInput.hpp"
#include "core/PlatformContext.hpp"
#include "core/App.hpp"
#include "core/Window.hpp"
#include "core/util/SceneNodeUtil.hpp"
#include <string>
#include <cassert>

// --- BMICalcScene: BMI計算シーンのSceneNodeベース実装 ---
class BMICalcScene : public Scene
{
public:
  // --- Eval functors for DerivedState ---
  struct HeightEval : public DerivedState<double>::EvalFn
  {
    MutableState<std::string> *str;
    HeightEval(MutableState<std::string> *s) : str(s) {}
    double operator()() { return BMICalcScene::strToDouble(str->get()); }
  };
  struct WeightEval : public DerivedState<double>::EvalFn
  {
    MutableState<std::string> *str;
    WeightEval(MutableState<std::string> *s) : str(s) {}
    double operator()() { return BMICalcScene::strToDouble(str->get()); }
  };
  struct BMIEval : public DerivedState<double>::EvalFn
  {
    MutableState<std::string> *hStr;
    MutableState<std::string> *wStr;
    BMIEval(MutableState<std::string> *h, MutableState<std::string> *w) : hStr(h), wStr(w) {}
    double operator()() { return BMICalcScene::BMIMultiArgCalcBMI(hStr->get(), wStr->get()); }
  };

  BMICalcScene()
      : Scene(new SceneHost()),
        heightStr("170.0"),
        weightStr("60.0"),
        height({&heightStr}, new HeightEval(&heightStr)),
        weight({&weightStr}, new WeightEval(&weightStr)),
        bmi({&heightStr, &weightStr}, new BMIEval(&heightStr, &weightStr)),
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
    group.add(NodeAs(TextDefinition(TextProps().setText("身長(cm)を入力してください"))));
    group.add(NodeAs(TextInputDefinition(TextInputProps().setText(&heightStr))));
    group.add(NodeAs(TextDefinition(TextProps().setText("体重(kg)を入力してください"))));
    group.add(NodeAs(TextInputDefinition(TextInputProps().setText(&weightStr))));
    group.add(NodeAs(TextDefinition(TextProps().setText("BMI: " + doubleToStr(bmi.get())))));
    // ...ボタン等追加可...
  }

  MutableState<std::string> heightStr;
  MutableState<std::string> weightStr;
  DerivedState<double> height;
  DerivedState<double> weight;
  DerivedState<double> bmi;
  PushStateTracker tracker;
};
