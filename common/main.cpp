#include "Property.hpp" // 基本クラスを含むヘッダを先に
#include "Tracker.hpp"  // 次にTrackerを含むヘッダ
#include "Page.hpp"     // より詳細なコンポーネント
#include "Button.hpp"
#include "Renderer.hpp"
#include "App.hpp"
#include <string>
#include <iostream>

// C++98互換: 2倍値計算用のグローバル関数
static int doubleFn(const int &v) { return v * 2; }

// StaticPropのグローバルインスタンス
StaticProp<bool> TRUE(true);
StaticProp<bool> FALSE(false);

class FormPage : public Page
{
public:
  FormPage()
      : Page(),
        name(""),
        isValid(&name, &FormPage::evaluateLength),
        tracker({&name}, {&isValid}) {}
  static bool evaluateLength(const std::string &s);
  static void onSendClick();
  void build(PageBuilder &b);
  State<std::string> name;
  DerivedProp<bool, std::string> isValid;
  StdTracker tracker;
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
class BMICalcPage : public Page
{
public:
  BMICalcPage()
      : Page(),
        heightStr("170.0"),
        weightStr("60.0"),
        height(&heightStr, strToDouble),
        weight(&weightStr, strToDouble),
        // BMIPropStructを使用して、複数引数から計算
        bmi({&heightStr, &weightStr}, BMIMultiArgCalcBMI),
        tracker({&heightStr, &weightStr}, {&height, &weight, &bmi})
  {
  }

  static double strToDouble(const std::string &s) { return atof(s.c_str()); }
  static std::string doubleToStr(const double &v)
  {
    char buf[32];
    sprintf(buf, "%.2f", v);
    return std::string(buf);
  }

  // 複数引数を持つ構造体への変換関数
  struct BMIArgs
  {
    std::string heightStr;
    std::string weightStr;
  };

  // DerivedPropStructのfillStruct特殊化
  class BMIPropStruct : public DerivedPropStruct<double, BMIArgs>
  {
  public:
    BMIPropStruct(const std::vector<StateBase *> &states, EvalFn eval)
        : DerivedPropStruct<double, BMIArgs>(states, eval),
          heightStrState((State<std::string> *)states[0]),
          weightStrState((State<std::string> *)states[1]) {}

  protected:
    BMIArgs getStruct() const override
    {
      BMIArgs args;
      args.heightStr = heightStrState->get();
      args.weightStr = weightStrState->get();
      return args;
    }

  private:
    State<std::string> *heightStrState;
    State<std::string> *weightStrState;
  };

  static double BMIMultiArgCalcBMI(const BMIArgs &args)
  {
    double h = strToDouble(args.heightStr);
    double w = strToDouble(args.weightStr);
    if (h <= 0)
      return 0.0;
    double m = h / 100.0;
    return w / (m * m);
  }

  static double calcBMI(const double &h, const double &w)
  {
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
  BMIPropStruct bmi; // 型を変更: 特定の実装クラスに
  StdTracker tracker;
};

class MyRenderer : public Renderer
{
  // ...仮実装...
};

void testTrackerPropagation()
{
  // 新設計に合わせて修正：先にState/Propを作成
  State<int> s_int(10);
  DerivedProp<int, int> doubleProp(&s_int, doubleFn);

  // 最後にStateとPropを配列でTrackerに渡す
  std::vector<StateBase *> states = {&s_int};
  std::vector<BindablePropBase *> props = {&doubleProp};
  StdTracker tracker(states, props);

  struct DoublePropCallback
  {
    static void onChange(int v, void *)
    {
      std::cout << "[DerivedProp] double value: " << v << std::endl;
    }
  };
  doubleProp.bind(DoublePropCallback::onChange, NULL, false);
  s_int.set(21);
  std::cout << "s_int: " << s_int.get() << std::endl;
  std::cout << "doubleProp: " << doubleProp.get() << std::endl;
}

void testDeferredSideEffect()
{
  // 新設計に合わせて修正
  State<int> s_int(5);
  DerivedProp<int, int> doubleProp(&s_int, doubleFn);

  // StateとPropを配列でTrackerに渡す
  std::vector<StateBase *> states = {&s_int};
  std::vector<BindablePropBase *> props = {&doubleProp};
  StdTracker tracker(states, props);

  struct DeferredCallback
  {
    static void onDeferred()
    {
      std::cout << "[deferred] UI redraw or log: commit completed!" << std::endl;
    }
  };
  tracker.defer(DeferredCallback::onDeferred);
  s_int.set(7);
  std::cout << "s_int: " << s_int.get() << std::endl;
  std::cout << "doubleProp: " << doubleProp.get() << std::endl;
}

// TextInputのonChange模擬イベント
void testTextInputOnChange()
{
  // 新設計に合わせて修正
  State<std::string> name("");
  DerivedProp<bool, std::string> isValid(&name, FormPage::evaluateLength);

  // StateとPropを配列でTrackerに渡す
  std::vector<StateBase *> states = {&name};
  std::vector<BindablePropBase *> props = {&isValid};
  StdTracker tracker(states, props);

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
  // 新設計に合わせて修正
  State<int> s1(1);
  State<int> s2(2);
  DerivedProp<int, int> sumProp(&s1, doubleFn); // s1のみ依存例

  // StateとPropを配列でTrackerに渡す
  std::vector<StateBase *> states = {&s1, &s2};
  std::vector<BindablePropBase *> props = {&sumProp};
  StdTracker tracker(states, props);

  tracker.begin();
  s1.set(10);
  s2.set(20);
  tracker.end(NULL);
  std::cout << "[Batch] s1=" << s1.get() << ", s2=" << s2.get() << ", sumProp=" << sumProp.get() << std::endl;
}

// RAIIスコープでのトランザクション例
void testRAIITransaction()
{
  // 新設計に合わせて修正
  State<int> s(0);
  DerivedProp<int, int> doubleProp(&s, doubleFn);

  // StateとPropを配列でTrackerに渡す
  std::vector<StateBase *> states = {&s};
  std::vector<BindablePropBase *> props = {&doubleProp};
  StdTracker tracker(states, props);

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

  // 新設計に合わせて修正：先にState/Propを作成
  State<std::string> name("");
  State<std::string> email("");
  State<int> age(0);
  State<bool> agree(false);

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

  // バリデーション関数 - スタティック関数をローカルスコープで定義できないのでグローバルにする
  struct FormValidator
  {
    static bool validate(const FormInputs &f)
    {
      return !f.name.empty() && !f.email.empty() && f.age >= 18 && f.agree;
    }
  };

  // DerivedPropStruct生成（新設計：Trackerなし）
  class MyDerivedPropStruct : public DerivedPropStruct<bool, FormInputs>
  {
  public:
    MyDerivedPropStruct(const std::vector<StateBase *> &s)
        : DerivedPropStruct<bool, FormInputs>(s, &FormValidator::validate),
          n((State<std::string> *)s[0]),
          e((State<std::string> *)s[1]),
          a((State<int> *)s[2]),
          g((State<bool> *)s[3]) {}

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

  // 新設計のMyDerivedPropStructを生成（Trackerなし）
  MyDerivedPropStruct isValid(deps);

  // Trackerを初期化しdepsとisValidをbind
  std::vector<BindablePropBase *> props = {&isValid};
  StdTracker tracker(deps, props);

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

int main()
{
  MyRenderer renderer;
  Window window(&renderer);
  FormPage page;
  window.setPage(&page);
  App app(&window);
  app.run();
  testTrackerPropagation();
  testDeferredSideEffect();
  testBatchTransaction();
  testRAIITransaction();
  testTextInputOnChange();
  testDerivedPropStruct();
  return 0;
}
