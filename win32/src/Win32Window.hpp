#ifndef DECLARA_WIN32WINDOW_HPP
#define DECLARA_WIN32WINDOW_HPP

#include "Window.hpp"
#include "Renderer.hpp"
#include <windows.h>

// 🦊 Win32固有のWindow実装
class Win32Window : public Window
{
public:
  Win32Window(Renderer *renderer, HWND hwnd = 0)
      : Window(renderer), hwnd_(hwnd) {}

  HWND hwnd() const { return hwnd_; }

  // 必要に応じてWindowの仮想関数をオーバーライド
  // 例:
  // void setPage(Page* page) override { /* TODO: 実装 */ }
  // void rerender() override { /* TODO: 実装 */ }

private:
  HWND hwnd_;
  void createNativeWindow();
  void destroyNativeWindow();
  void onVisibilityChanged(bool visible);                           // visibility変更時のコールバック
  static void VisibilityChangedThunk(bool visible, void *userData); // BindableProp<bool>用thunk
  // 他にもWin32固有のメンバを追加可能
};

#endif // DECLARA_WIN32WINDOW_HPP
