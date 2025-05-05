#ifndef DECLARA_WIN32WINDOW_HPP
#define DECLARA_WIN32WINDOW_HPP

#include "Window.hpp"
#include "Renderer.hpp"
#include <windows.h>

class Win32App;

// 🦊 Win32固有のWindow実装
class Win32Window : public Window
{
public:
  // コンストラクタ引数に Win32App* を追加
  Win32Window(Win32App *app, Renderer *renderer, HWND hwnd = 0);

  HWND hwnd() const { return hwnd_; }

  // --- 追加: Win32 WndProc ---
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  // 必要に応じてWindowの仮想関数をオーバーライド
  // 例:
  // void setPage(Page* page) override { /* TODO: 実装 */ }
  // void rerender() override { /* TODO: 実装 */ }

protected:
  HWND hwnd_;
  Win32App *app_; // <-- Win32Appへのポインタを追加

private:
  void createNativeWindow();
  void destroyNativeWindow();
  void onVisibilityChanged(bool visible);             // visibility変更時のコールバック
  static void VisibilityChangedThunk(void *userData); // State<bool>用thunk
  // 他にもWin32固有のメンバを追加可能
};

#endif // DECLARA_WIN32WINDOW_HPP
