#include "Page.hpp"
#include "Button.hpp"
#include "Renderer.hpp"
#include "App.hpp"
#include "Property.hpp"
#include "Tracker.hpp"
#include <string>
#include <iostream>

// C++98互換: 2倍値計算用のグローバル関数
static int doubleFn(const int &v) { return v * 2; }

class FormPage : public Page
{
public:
  FormPage() : Page(), name(&transaction_, ""), isValid(&transaction_, &name, &FormPage::evaluateLength) {}
  static bool evaluateLength(const std::string &s);
  static void onSendClick();
  void build(PageBuilder &b);
  State<std::string> name;
  DerivedProp<bool, std::string> isValid;
};
bool FormPage::evaluateLength(const std::string &s) { return s.length() >= 3; }
void FormPage::onSendClick() {}
void FormPage::build(PageBuilder &b)
{
  b.Text("名前を入力してください");
  b.TextInput(&name);
  b.Button(ButtonOptions().setLabel("送信").setEnabled(&isValid).setOnClick(&FormPage::onSendClick));
}

// --- BMICalcPage: BMI計算ページの例 ---
// convertablestate.md方針に従い、ConvertableState廃止＋DerivedPropで整理
static double strToDouble(const std::string &s) { return atof(s.c_str()); }
static std::string doubleToStr(const double &v)
{
  char buf[32];
  sprintf(buf, "%.2f", v);
  return std::string(buf);
}

class BMICalcPage : public Page
{
public:
  BMICalcPage()
      : Page(),
        heightStr(&transaction_, "170.0"),
        weightStr(&transaction_, "60.0"),
        height(&transaction_, &heightStr, strToDouble),
        weight(&transaction_, &weightStr, strToDouble),
        bmi(&transaction_, &height, BMICalcPage::calcBMI) {}

  static double calcBMI(const double &h)
  {
    // 仮: 体重は固定値で
    double w = 60.0;
    if (h <= 0)
      return 0.0;
    double m = h / 100.0;
    return w / (m * m);
  }
  void build(PageBuilder &b)
  {
    b.Text("身長(cm)を入力してください");
    b.TextInput(&heightStr);
    b.Text("体重(kg)を入力してください");
    b.TextInput(&weightStr);
    b.Text("BMI: " + doubleToStr(bmi.get()));
    // ...ボタン等...
  }
  State<std::string> heightStr;
  State<std::string> weightStr;
  DerivedProp<double, std::string> height;
  DerivedProp<double, std::string> weight;
  DerivedProp<double, double> bmi;
};

class MyRenderer : public Renderer
{
  // ...仮実装...
};

void testStatePolymorphism()
{
  State<int> s_int(nullptr, 42);
  State<bool> s_bool(nullptr, true);
  State<std::string> s_str(nullptr, "senko");
  struct MyUrl
  {
    std::string url;
    MyUrl(const std::string &u) : url(u) {}
  };
  State<MyUrl> s_url(nullptr, MyUrl("https://senko.dev"));

  // 型消去で値をセット
  s_int.setValue(ValueHolder<int>(123));
  s_bool.setValue(ValueHolder<bool>(false));
  s_str.setValue(ValueHolder<std::string>("もふもふ"));
  s_url.setValue(ValueHolder<MyUrl>(MyUrl("https://mofmof.ai")));

  std::cout << "int: " << s_int.get() << std::endl;
  std::cout << "bool: " << (s_bool.get() ? "true" : "false") << std::endl;
  std::cout << "str: " << s_str.get() << std::endl;
  std::cout << "url: " << s_url.get().url << std::endl;
}

void testTrackerPropagation()
{
  StdTracker tracker;
  State<int> s_int(NULL, 10);
  DerivedProp<int, int> doubleProp(&tracker, &s_int, doubleFn);
  struct DoublePropCallback
  {
    static void onChange(int v, void *)
    {
      std::cout << "[DerivedProp] double value: " << v << std::endl;
    }
  };
  doubleProp.bind(DoublePropCallback::onChange, NULL, false);
  s_int.setValue(21);
  std::cout << "s_int: " << s_int.get() << std::endl;
  std::cout << "doubleProp: " << doubleProp.get() << std::endl;
}

void testDeferredSideEffect()
{
  StdTracker tracker;
  State<int> s_int(NULL, 5);
  DerivedProp<int, int> doubleProp(&tracker, &s_int, doubleFn);
  struct DeferredCallback
  {
    static void onDeferred()
    {
      std::cout << "[deferred] UI redraw or log: commit completed!" << std::endl;
    }
  };
  tracker.defer(DeferredCallback::onDeferred);
  s_int.setValue(7);
  std::cout << "s_int: " << s_int.get() << std::endl;
  std::cout << "doubleProp: " << doubleProp.get() << std::endl;
}

// TextInputのonChange模擬イベント
void testTextInputOnChange()
{
  StdTracker tracker;
  State<std::string> name(&tracker, "");
  DerivedProp<bool, std::string> isValid(&tracker, &name, FormPage::evaluateLength);
  struct ValidCallback
  {
    static void onChange(bool v, void *)
    {
      std::cout << "[isValid] changed: " << (v ? "OK" : "NG") << std::endl;
    }
  };
  isValid.bind(ValidCallback::onChange, NULL, false);

  // ユーザーがTextInputで値を入力したと仮定
  std::cout << "[TextInput] name.set(\"ab\")" << std::endl;
  name.set("ab"); // 3文字未満→NG
  std::cout << "isValid: " << (isValid.get() ? "OK" : "NG") << std::endl;

  std::cout << "[TextInput] name.set(\"senko\")" << std::endl;
  name.set("senko"); // 3文字以上→OK
  std::cout << "isValid: " << (isValid.get() ? "OK" : "NG") << std::endl;
}

// RAIIによる自動トランザクション管理
struct AutoTransactionGuard
{
  StdTracker *tracker;
  AutoTransactionGuard(StdTracker *t) : tracker(t) { tracker->begin(); }
  ~AutoTransactionGuard() { tracker->end(nullptr); }
};

// 複数変更をまとめてコミットする例
void testBatchTransaction()
{
  StdTracker tracker;
  State<int> s1(NULL, 1);
  State<int> s2(NULL, 2);
  DerivedProp<int, int> sumProp(&tracker, &s1, doubleFn); // s1のみ依存例
  tracker.begin();
  s1.set(10);
  s2.set(20);
  tracker.end(NULL);
  std::cout << "[Batch] s1=" << s1.get() << ", s2=" << s2.get() << ", sumProp=" << sumProp.get() << std::endl;
}

// RAIIスコープでのトランザクション例
void testRAIITransaction()
{
  StdTracker tracker;
  State<int> s(NULL, 0);
  DerivedProp<int, int> doubleProp(&tracker, &s, doubleFn);
  {
    AutoTransactionGuard _(&tracker);
    s.set(50);
  }
  std::cout << "[RAII] s=" << s.get() << ", doubleProp=" << doubleProp.get() << std::endl;
}

// --- 構造体まとめ方式（DerivedPropStruct）サンプル ---
void testDerivedPropStruct()
{
  struct FormInputs
  {
    std::string name;
    std::string email;
    int age;
    bool agree;
  };
  // State群
  State<std::string> name(NULL, "");
  State<std::string> email(NULL, "");
  State<int> age(NULL, 0);
  State<bool> agree(NULL, false);
  // StateBase*配列
  std::vector<StateBase *> deps;
  deps.push_back(&name);
  deps.push_back(&email);
  deps.push_back(&age);
  deps.push_back(&agree);
  // 構造体に値を詰める関数（C++98:手動で）
  struct FormStructUtil
  {
    static FormInputs make(const State<std::string> &n, const State<std::string> &e, const State<int> &a, const State<bool> &g)
    {
      FormInputs f;
      f.name = n.get();
      f.email = e.get();
      f.age = a.get();
      f.agree = g.get();
      return f;
    }
  };
  // バリデーション関数
  static bool validate(const FormInputs &f)
  {
    return !f.name.empty() && !f.email.empty() && f.age >= 18 && f.agree;
  }
  // DerivedPropStruct生成
  class MyDerivedPropStruct : public DerivedPropStruct<bool, FormInputs>
  {
  public:
    MyDerivedPropStruct(Tracker *t, const std::vector<StateBase *> &s)
        : DerivedPropStruct<bool, FormInputs>(t, s, &validate), n((State<std::string> *)s[0]), e((State<std::string> *)s[1]), a((State<int> *)s[2]), g((State<bool> *)s[3]) {}

  protected:
    FormInputs getStruct() const
    {
      return FormStructUtil::make(*n, *e, *a, *g);
    }
    State<std::string> *n;
    State<std::string> *e;
    State<int> *a;
    State<bool> *g;
  };
  StdTracker tracker;
  MyDerivedPropStruct isValid(&tracker, deps);
  struct Callback
  {
    static void onChange(bool v, void *) { std::cout << "[Struct] isValid: " << (v ? "OK" : "NG") << std::endl; }
  };
  isValid.bind(Callback::onChange, NULL, false);
  // 値を変えてみる
  name.set("senko");
  email.set("");
  age.set(20);
  agree.set(true);       // email空→NG
  email.set("senko@ai"); // OK
  age.set(15);           // NG
  age.set(22);           // OK
  agree.set(false);      // NG
  agree.set(true);       // OK
}

// --- テンプレート量産方式（DerivedProp2/3）サンプル ---
void testDerivedProp2()
{
  State<int> a(NULL, 1);
  State<int> b(NULL, 2);
  static int sumFn(const int &x, const int &y) { return x + y; }
  DerivedProp2<int, int, int> sumProp(NULL, &a, &b, sumFn);
  struct Callback
  {
    static void onChange(int v, void *) { std::cout << "[Prop2] sum: " << v << std::endl; }
  };
  sumProp.bind(Callback::onChange, NULL, false);
  a.set(10);
  b.set(20);
  a.set(5);
  b.set(7);
}
void testDerivedProp3()
{
  State<int> x(NULL, 1), y(NULL, 2), z(NULL, 3);
  static int prodFn(const int &a, const int &b, const int &c) { return a * b * c; }
  DerivedProp3<int, int, int, int> prodProp(NULL, &x, &y, &z, prodFn);
  struct Callback
  {
    static void onChange(int v, void *) { std::cout << "[Prop3] prod: " << v << std::endl; }
  };
  prodProp.bind(Callback::onChange, NULL, false);
  x.set(2);
  y.set(3);
  z.set(4);
  x.set(1);
  y.set(1);
  z.set(1);
}

int main()
{
  MyRenderer renderer;
  Window window(&renderer);
  FormPage page;
  window.setPage(&page);
  App app(&window);
  app.run();
  testStatePolymorphism();
  testTrackerPropagation();
  testDeferredSideEffect();
  testBatchTransaction();
  testRAIITransaction();
  testTextInputOnChange();
  testDerivedPropStruct();
  testDerivedProp2();
  testDerivedProp3();
  return 0;
}
