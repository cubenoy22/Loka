# BMI 計算アプリ実装計画

## 🎯 目標

最小限の宣言的 UI ライブラリで動作する **BMI 計算アプリ** を作成し、以下を検証：

- State + DerivedState による自動再計算
- EditText → State の双方向バインド
- Text への State バインド
- NodeComposition による宣言的 UI 構築
- プラットフォーム固有の NodeContext 実装

---

## 📋 必要な機能（優先度順）

### Phase 1: コア機能（1-2 週間）

#### 1.1 EditText の実装

```cpp
// Props/Node/Definition
struct EditTextProps : public NodePropsBase<EditTextProps> {
  State<std::string>* text;  // 双方向バインド
  State<std::string>* placeholder;
};

class EditTextNode : public Node {
  const EditTextProps& props;
};

typedef EditTextDefinition EditText;
```

**OS 側実装（Win32 例）:**

```cpp
class Win32EditTextContext : public NodeContext {
  HWND hwnd;
  const EditTextProps& props;

  Win32EditTextContext(const EditTextProps& p) : props(p) {
    hwnd = CreateWindow("EDIT", ...);

    // State → OS (一方向)
    if (props.text) {
      props.text->bind(&Win32EditTextContext::updateText, this);
    }
  }

  static void updateText(void* ctx) {
    Win32EditTextContext* self = (Win32EditTextContext*)ctx;
    SetWindowText(self->hwnd, self->props.text->get().c_str());
  }

  // OS → State (逆方向)
  void onTextChanged() {
    char buf[256];
    GetWindowText(hwnd, buf, 256);
    if (props.text) {
      // MutableStateにキャストして set
      MutableState<std::string>* mut =
        dynamic_cast<MutableState<std::string>*>(props.text);
      if (mut) mut->set(std::string(buf));
    }
  }
};
```

#### 1.2 Text の実装

```cpp
struct TextProps : public NodePropsBase<TextProps> {
  State<std::string>* text;
};

class TextNode : public Node {
  const TextProps& props;
};

typedef TextDefinition Text;
```

**OS 側実装（Win32 例）:**

```cpp
class Win32TextContext : public NodeContext {
  HWND hwnd;
  const TextProps& props;

  Win32TextContext(const TextProps& p) : props(p) {
    hwnd = CreateWindow("STATIC", ...);

    // State → OS (一方向のみ)
    if (props.text) {
      props.text->bind(&Win32TextContext::updateText, this);
    }
  }

  static void updateText(void* ctx) {
    Win32TextContext* self = (Win32TextContext*)ctx;
    SetWindowText(self->hwnd, self->props.text->get().c_str());
  }
};
```

#### 1.3 DerivedState による BMI 計算

```cpp
// BMI計算用のEvalFn
struct BMICalc : DerivedState<float>::EvalFn {
  State<float>* weight;
  State<float>* height;

  BMICalc(State<float>* w, State<float>* h) : weight(w), height(h) {}

  float operator()() override {
    float w = weight->get();
    float h = height->get();
    if (h > 0) return w / (h * h);
    return 0.0f;
  }
};

// 使用例
MutableState<float> weight(60.0f);
MutableState<float> height(1.70f);

std::vector<StateBase*> deps;
deps.push_back(&weight);
deps.push_back(&height);

BMICalc* calc = new BMICalc(&weight, &height);
DerivedState<float> bmi(deps, calc);
```

---

### Phase 2: Window 統合（1 週間）

#### 2.1 Window::mount() の実装

```cpp
class Window {
  NodeComposition composition;
  HWND hwnd;  // Win32例
  std::vector<NodeContext*> contexts;  // 生成したContext管理

public:
  Window() {
    // Win32ウィンドウ作成
    hwnd = CreateWindow("DeclaraWindow", "BMI Calculator",
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
                        NULL, NULL, NULL, NULL);
  }

  template<class RootNode>
  void mount(RootNode& root) {
    // 1. Arena構築
    root.compose(composition);

    // 2. Node ツリー生成
    Node* rootNode = composition.createNodeTree();

    // 3. Host生成 & Binder接続
    buildHostTree(rootNode, hwnd);

    // 4. 表示
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
  }

  ~Window() {
    // Contextクリーンアップ
    for (size_t i = 0; i < contexts.size(); ++i) {
      delete contexts[i];
    }
  }

private:
  void buildHostTree(Node* node, HWND parent);
};
```

#### 2.2 NodeContext ファクトリ

```cpp
NodeContext* createNodeContext(Node* node, HWND parent) {
  // node の型に応じて適切な Context を生成
  if (ButtonNode* btn = dynamic_cast<ButtonNode*>(node)) {
    return new Win32ButtonContext(btn->props, parent);
  }
  if (EditTextNode* edit = dynamic_cast<EditTextNode*>(node)) {
    return new Win32EditTextContext(edit->props, parent);
  }
  if (TextNode* text = dynamic_cast<TextNode*>(node)) {
    return new Win32TextContext(text->props, parent);
  }
  return nullptr;
}

void Window::buildHostTree(Node* node, HWND parent) {
  if (!node) return;

  // NodeContextを生成
  NodeContext* ctx = createNodeContext(node, parent);
  if (ctx) {
    node->context = ctx;
    contexts.push_back(ctx);  // 管理用に保存
  }

  // 子ノードを再帰的に処理
  // TODO: Nodeの子走査ロジック実装
}
```

---

### Phase 3: BMI 計算アプリ本体（数日）

#### 3.1 アプリケーションクラス

```cpp
class BMICalculatorApp : public CompositeNode {
  // 数値State
  MutableState<float> weight;
  MutableState<float> height;
  DerivedState<float> bmi;

  // 文字列State（UI表示用）
  MutableState<std::string> weightStr;
  MutableState<std::string> heightStr;
  MutableState<std::string> bmiStr;

  // ラベル用の静的State
  State<std::string> weightLabel;
  State<std::string> heightLabel;
  State<std::string> bmiLabel;

public:
  BMICalculatorApp()
    : weight(60.0f)
    , height(1.70f)
    , bmi(makeBMICalc())
    , weightStr("60.0")
    , heightStr("1.70")
    , bmiStr("")
    , weightLabel("体重 (kg):")
    , heightLabel("身長 (m):")
    , bmiLabel("BMI:")
  {
    // 数値 ↔ 文字列の相互変換を設定
    weightStr.deferBind(&BMICalculatorApp::onWeightStrChanged, this);
    heightStr.deferBind(&BMICalculatorApp::onHeightStrChanged, this);
    bmi.bind(&BMICalculatorApp::onBMIChanged, this);
  }

  void compose(NodeComposition& c) override {
    // 体重入力
    c.declare(Text().setText(&weightLabel));
    c.declare(EditText().setText(&weightStr));

    // 身長入力
    c.declare(Text().setText(&heightLabel));
    c.declare(EditText().setText(&heightStr));

    // BMI結果表示
    c.declare(Text().setText(&bmiLabel));
    c.declare(Text().setText(&bmiStr));
  }

private:
  DerivedState<float> makeBMICalc() {
    std::vector<StateBase*> deps;
    deps.push_back(&weight);
    deps.push_back(&height);
    BMICalc* calc = new BMICalc(&weight, &height);
    return DerivedState<float>(deps, calc);
  }

  static void onWeightStrChanged(void* self) {
    BMICalculatorApp* app = (BMICalculatorApp*)self;
    float val = atof(app->weightStr.get().c_str());
    app->weight.set(val);
  }

  static void onHeightStrChanged(void* self) {
    BMICalculatorApp* app = (BMICalculatorApp*)self;
    float val = atof(app->heightStr.get().c_str());
    app->height.set(val);
  }

  static void onBMIChanged(void* self) {
    BMICalculatorApp* app = (BMICalculatorApp*)self;
    char buf[64];
    sprintf(buf, "BMI: %.1f", app->bmi.get());
    app->bmiStr.set(std::string(buf));
  }
};
```

#### 3.2 main 関数

```cpp
#include <windows.h>
#include "BMICalculatorApp.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  // Windowクラスの登録
  WNDCLASS wc = {};
  wc.lpfnWndProc = DefWindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = "DeclaraWindow";
  RegisterClass(&wc);

  // Windowとアプリ生成
  Window window;
  BMICalculatorApp app;

  // マウント
  window.mount(app);

  // メッセージループ
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}
```

---

## ✅ 実装チェックリスト

### Week 1-2: コアコンポーネント

- [ ] **EditTextProps/Node/Definition** 定義
- [ ] **TextProps/Node/Definition** 定義
- [ ] **Win32EditTextContext** 実装（双方向バインド）
- [ ] **Win32TextContext** 実装
- [ ] **DerivedState** の BMI 計算動作確認
- [ ] **文字列 ↔ 数値変換** ロジック実装

### Week 3: Window 統合

- [ ] **Window::mount()** 実装
- [ ] **NodeContext ファクトリ** 実装
- [ ] **buildHostTree()** 実装（Arena → Host 生成パイプライン）
- [ ] **メッセージループ統合**
- [ ] **Win32 メッセージハンドラ** 実装（EditText 用）

### Week 4: アプリケーション完成

- [ ] **BMICalculatorApp** クラス実装
- [ ] **レイアウト調整**（手動配置 or BoxLayout）
- [ ] **動作確認・デバッグ**
- [ ] **エラーハンドリング**（0 除算等）
- [ ] **UI 改善**（単位表示、結果の分類等）

---

## 🎓 学べること

### 1. State 駆動 UI

値の変更が自動的に UI に反映される仕組みを体験

```cpp
weight.set(70.0f);  // → BMI自動再計算 → UI自動更新
```

### 2. DerivedState

依存関係に基づく自動再計算

```cpp
DerivedState<float> bmi(deps, calc);  // weight/height変更で自動再計算
```

### 3. 双方向バインド

EditText ↔ State の連携

```cpp
// State → UI
props.text->bind(&updateText, this);
// UI → State
mut->set(newValue);
```

### 4. Arena パターン

一時オブジェクトの効率的管理

```cpp
c.declare(Text().setText(&label));  // 一時オブジェクトをArenaにコピー
```

### 5. NodeContext

プラットフォーム固有実装の隔離

```cpp
class Win32EditTextContext : public NodeContext { /* OS固有 */ };
```

---

## 🚀 次のステップ

BMI 計算アプリ完成後に取り組むこと：

### 短期（1-2 週間）

1. **BoxLayout** の deferred-bounds 実装
2. **StateTracker** との統合強化
3. **複数コンポーネント** の compose 対応

### 中期（1 ヶ月）

4. **HeadlessNode** による非同期計算
5. **EmitterState** による Button イベント実装
6. **より複雑なアプリ**（ToDo リスト等）

### 長期（2-3 ヶ月）

7. **macOS 移植** (NSView/NSControl)
8. **Classic Mac 対応** (68k/PPC)
9. **State Variants** (Debounce/Throttle)

---

## 💡 実装のヒント

### デバッグ方法

```cpp
// State変化のログ出力
weight.bind([](void* ctx) {
  printf("Weight changed: %.1f\n", ((State<float>*)ctx)->get());
}, &weight);
```

### エラーハンドリング

```cpp
float operator()() override {
  float w = weight->get();
  float h = height->get();
  if (h <= 0 || w <= 0) return 0.0f;  // ← ガード
  return w / (h * h);
}
```

### レイアウトのヒント（手動配置版）

```cpp
// Win32での位置指定
SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOZORDER);
```

---

## 参考資料

- [declara_design_minutes.md](./declara_design_minutes.md) - 設計書本体
- `common/core/State.hpp` - State 実装
- `common/core2/scene/Node.hpp` - Node/Props 基盤
- `common/app2/Button.hpp` - Button 実装例
