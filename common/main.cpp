#include "core/Tracker.hpp"
#include "core/Scene.hpp"
#include "app/Button.hpp"
#include "app/Renderer.hpp"
#include "core/App.hpp"
#include "core/Window.hpp"
#include <string>
#include <iostream>
#include <cassert>

// C++98互換: 2倍値計算用のグローバル関数
static int doubleFn(const int &v) { return v * 2; }

// --- State/DerivedState/MutableState/Trackerベースに全面リファクタ ---

// グローバル定数State
State<bool> TRUE(true);
State<bool> FALSE(false);

class FormScene : public Scene
{
public:
  FormScene()
      : Scene(),
        name(""),
        isValid({&name}, [&]()
                { return name.get().length() >= 3; }),
        tracker({&name, &isValid}) {}
  static bool evaluateLength(const std::string &s) { return s.length() >= 3; }
  static void onSendClick() {}
  void build(SceneBuilder &b)
  {
    b.Text("名前を入力してください");
    b.TextInput(&name);
    b.Button(ButtonOptions().setLabel("送信").setEnabled(&isValid).setOnClick(&FormScene::onSendClick));
  }
  MutableState<std::string> name;
  DerivedState<bool> isValid;
  StdTracker tracker;
};

// --- BMICalcScene: BMI計算シーンの例 ---
class BMICalcScene : public Scene
{
public:
  BMICalcScene()
      : Scene(),
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
  void build(SceneBuilder &b)
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
  StdTracker tracker;
};

class MyRenderer : public Renderer
{
  // ...仮実装...
};

void testTrackerPropagation()
{
  MutableState<int> s_int(10);
  DerivedState<int> doubleProp({&s_int}, [&]()
                               { return s_int.get() * 2; });
  StdTracker tracker({&s_int, &doubleProp});
  struct DoublePropCallback
  {
    static void onChange(int v, void *)
    {
      std::cout << "[DerivedState] double value: " << v << std::endl;
    }
  };
  doubleProp.bind((StateBase::OnChangeFn)DoublePropCallback::onChange, NULL, false);
  tracker.begin();
  s_int.set(21);
  tracker.end();
  std::cout << "s_int: " << s_int.get() << std::endl;
  std::cout << "doubleProp: " << doubleProp.get() << std::endl;
}

void testDeferredSideEffect()
{
  MutableState<int> s_int(5);
  DerivedState<int> doubleProp({&s_int}, [&]()
                               { return s_int.get() * 2; });
  StdTracker tracker({&s_int, &doubleProp});
  struct DeferredCallback
  {
    static void onDeferred(void *)
    {
      std::cout << "[deferred] UI redraw or log: commit completed!" << std::endl;
    }
  };
  tracker.defer(DeferredCallback::onDeferred, NULL);
  tracker.begin();
  s_int.set(7);
  tracker.end();
  std::cout << "s_int: " << s_int.get() << std::endl;
  std::cout << "doubleProp: " << doubleProp.get() << std::endl;
}

void testTextInputOnChange()
{
  MutableState<std::string> name("");
  DerivedState<bool> isValid({&name}, [&]()
                             { return FormScene::evaluateLength(name.get()); });
  StdTracker tracker({&name, &isValid});
  struct ValidCallback
  {
    static void onChange(void *userData)
    {
      DerivedState<bool> *isValid = (DerivedState<bool> *)userData;
      std::cout << "[isValid] changed: " << (isValid->get() ? "OK" : "NG") << std::endl;
    }
  };
  isValid.bind(ValidCallback::onChange, &isValid, false);
  std::cout << "[TextInput] name.set(\"ab\")" << std::endl;
  name.set("ab");
  std::cout << "isValid: " << (isValid.get() ? "OK" : "NG") << std::endl;
  std::cout << "[TextInput] name.set(\"senko\")" << std::endl;
  name.set("senko");
  std::cout << "isValid: " << (isValid.get() ? "OK" : "NG") << std::endl;
}

struct AutoTransactionGuard
{
  StdTracker *tracker;
  AutoTransactionGuard(StdTracker *t) : tracker(t) { tracker->begin(); }
  ~AutoTransactionGuard() { tracker->end(); }
};

void testBatchTransaction()
{
  MutableState<int> s1(1);
  MutableState<int> s2(2);
  DerivedState<int> sumProp({&s1}, [&]()
                            { return s1.get() * 2; });
  StdTracker tracker({&s1, &s2, &sumProp});
  tracker.begin();
  s1.set(10);
  s2.set(20);
  tracker.end();
  std::cout << "[Batch] s1=" << s1.get() << ", s2=" << s2.get() << ", sumProp=" << sumProp.get() << std::endl;
}

void testRAIITransaction()
{
  MutableState<int> s(0);
  DerivedState<int> doubleProp({&s}, [&]()
                               { return s.get() * 2; });
  StdTracker tracker({&s, &doubleProp});
  {
    AutoTransactionGuard _(&tracker);
    s.set(50);
  }
  std::cout << "[RAII] s=" << s.get() << ", doubleProp=" << doubleProp.get() << std::endl;
}

void testDerivedStruct()
{
  struct FormInputs
  {
    std::string name;
    std::string email;
    int age;
    bool agree;
  };
  MutableState<std::string> name("");
  MutableState<std::string> email("");
  MutableState<int> age(0);
  MutableState<bool> agree(false);
  std::vector<StateBase *> deps = {&name, &email, &age, &agree};
  DerivedState<bool> isValid(
      deps,
      [&]()
      {
        FormInputs f = {name.get(), email.get(), age.get(), agree.get()};
        return !f.name.empty() && !f.email.empty() && f.age >= 18 && f.agree;
      });
  StateBase *deps2[5];
  for (size_t i = 0; i < deps.size(); ++i)
    deps2[i] = deps[i];
  deps2[deps.size()] = &isValid;
  StdTracker tracker(std::vector<StateBase *>(deps2, deps2 + 5));
  struct Callback
  {
    static void onChange(bool v, void *)
    {
      std::cout << "[isValid] changed: " << v << std::endl;
    }
  };
  isValid.bind((StateBase::OnChangeFn)Callback::onChange, &isValid, false);
  tracker.begin();
  name.set("senko");
  email.set("");
  age.set(20);
  agree.set(true);
  tracker.end();

  tracker.begin();
  email.set("senko@ai");
  age.set(15);
  tracker.end();

  tracker.begin();
  age.set(22);
  agree.set(false);
  tracker.end();

  tracker.begin();
  agree.set(true);
  tracker.end();
}

// モック用の具象App実装
class MockApp : public App
{
public:
  MockApp(Window *w) : App(w) {}

  // App::quit の実装
  void quit() override
  {
    // モック実装、何もしない
  }

  // App::windowClosed の実装
  void windowClosed(Window *window) override
  {
    // モック実装、基底クラスの実装を呼び出す
    App::windowClosed(window);
  }
};

// int main()
// {
//   MyRenderer renderer;
//   Window window(&renderer, nullptr);
//   FormScene scene;
//   window.setScene(&scene);
//   MockApp app(&window);
//   app.run();
//   testTrackerPropagation();
//   testDeferredSideEffect();
//   testBatchTransaction();
//   testRAIITransaction();
//   testTextInputOnChange();
//   testDerivedStruct();

//   return 0;
// }
