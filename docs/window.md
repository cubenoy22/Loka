# Window ライフサイクル・イベント設計とプラットフォーム差分

---

## 概要

本ドキュメントでは、`Window` クラスの onCreate / onShow / onHide / onDestroy 各イベントの設計意図と、Win32・Toolbox（Mac OS Classic）・Cocoa（macOS）間の挙動差異についてまとめます。

---

## ライフサイクルイベントの定義

| イベント名    | 呼び出しタイミング（Win32 実装例）         | 役割・備考                                 |
| :------------ | :----------------------------------------- | :----------------------------------------- |
| `onCreate()`  | ネイティブウィンドウ生成直後               | リソース初期化、UI 部品の生成など          |
| `onShow()`    | `visibility` が `true` になり、表示直前    | `ShowWindow(hwnd, SW_SHOW)` 直前に呼ばれる |
| `onHide()`    | `visibility` が `false` になり、破棄直前   | `destroyNativeWindow()` 直前に呼ばれる     |
| `onDestroy()` | ウィンドウ破棄処理（例: `WM_DESTROY`）直前 | クリーンアップ、リソース解放など           |

---

## プラットフォームごとの挙動差分

### Win32 (Windows API)

- **onCreate**: `CreateWindowA` 直後、ウィンドウハンドル有効時に呼ばれる。
- **onShow**: `visibility` が true になったとき、`ShowWindow(hwnd, SW_SHOW)` 直前に呼ばれる。
- **onHide**: `visibility` が false になったとき、`destroyNativeWindow()` 直前に呼ばれる（Win32 では非表示 ≒ 破棄）。
- **onDestroy**: `WM_DESTROY` メッセージ受信時、ハンドル有効なうちに呼ばれる。
- **注意**: Win32 では「非表示」と「破棄」が密接。`ShowWindow(hwnd, SW_HIDE)` は一時的な非表示用途。

### Toolbox (Mac OS Classic)

- **onCreate**: `NewWindow` 直後、WindowPtr 有効時に呼ばれる。
- **onShow**: `ShowWindow` 直前に呼ばれる。Toolbox ではウィンドウの可視/不可視を明示的に切り替え可能。
- **onHide**: `HideWindow` 直前に呼ばれる。ウィンドウは不可視状態でも生存し続ける。
- **onDestroy**: `DisposeWindow` 直前に呼ばれる。
- **注意**: Toolbox では「非表示」と「破棄」が明確に分離されている。

### Cocoa (macOS)

- **onCreate**: `NSWindow` インスタンス生成直後に呼ばれる。
- **onShow**: `[window makeKeyAndOrderFront:nil]` 直前に呼ばれる。
- **onHide**: `[window orderOut:nil]` 直前に呼ばれる。ウィンドウは不可視状態でも生存。
- **onDestroy**: `close`/`dealloc` 直前に呼ばれる。
- **注意**: Cocoa も Toolbox 同様、「非表示」と「破棄」は分離。

---

## 設計指針まとめ

- **各イベントは「状態変化の直前」に呼ぶ**
- **Win32 は「非表示 ≒ 破棄」だが、Toolbox/Cocoa は「非表示」と「破棄」が分離**
- **プラットフォームごとの違いを吸収するため、イベントハンドラは必ず override で明示的に実装すること**
- **状態管理は State/StateTracker 経由で宣言的に行う**

---

## 参考: 実装例（Win32Window）

```cpp
void Win32Window::VisibilityChangedThunk(void *userData) {
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (!self) return;
  bool visible = self->visibility.get();
  if (visible) {
    if (!self->hwnd_) self->createNativeWindow();
    if (self->hwnd_) self->onShow();
  } else {
    if (self->hwnd_) {
      self->onHide();
      self->destroyNativeWindow();
    }
  }
}
```

---

## 今後の拡張・注意点

- 他プラットフォーム実装時は、各 API の「可視/不可視」「破棄」挙動を必ず確認し、onShow/onHide/onDestroy の呼び出しタイミングを調整すること。
- 状態遷移の一貫性を保つため、State/StateTracker のトランザクション管理も徹底すること。

---

以上。
