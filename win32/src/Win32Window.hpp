#ifndef DECLARA_WIN32WINDOW_HPP
#define DECLARA_WIN32WINDOW_HPP

#include "core/Window.hpp"
#include "app/Renderer.hpp"
#include <windows.h>

class Win32App;

// 🦊 Win32固有のWindow実装
class Win32Window : public Window
{
public:
  // WindowOptionsのtitleを受け取れるように拡張
  Win32Window(Win32App *app, Renderer *renderer, const std::string &title, HWND hwnd = 0);

  HWND hwnd() const { return hwnd_; }

  // --- 追加: Win32 WndProc ---
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  // 必要に応じてWindowの仮想関数をオーバーライド
  // 例:
  // void setPage(Page* page) override { /* TODO: 実装 */ }
  // void rerender() override { /* TODO: 実装 */ }

protected:
  HWND hwnd_;
  HWND buttonHwnd_; // --- 追加: ボタン用HWND
  Win32App *app_;   // <-- Win32Appへのポインタを追加

private:
  void createNativeWindow();
  void destroyNativeWindow();
  void onVisibilityChanged(bool visible); // visibility変更時のコールバック
  static void VisibilityChangedThunk(void *userData);
  static void TitleChangedThunk(void *userData);
};

#endif // DECLARA_WIN32WINDOW_HPP
