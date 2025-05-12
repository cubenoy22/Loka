#ifndef DECLARA_WIN32WINDOW_HPP
#define DECLARA_WIN32WINDOW_HPP

#include "core/Window.hpp"
#include <windows.h>
#include <string>

class Win32App;
class PlatformContext;

// 🦊 Win32固有のWindow実装
class Win32Window : public Window
{
public:
  // WindowOptionsのtitle/visibleを受け取れるように拡張
  Win32Window(Win32App *app, PlatformContext *context, const std::string &title, HWND hwnd = 0, bool visible = true);

  HWND hwnd() const { return hwnd_; }

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  virtual void onRun();

protected:
  HWND hwnd_;
  HWND buttonHwnd_; // --- 追加: ボタン用HWND
  Win32App *app_;   // <-- Win32Appへのポインタを追加

private:
  void createNativeWindow();
  void destroyNativeWindow();
  static void VisibilityChangedThunk(void *userData);
  static void TitleChangedThunk(void *userData);
};

#endif // DECLARA_WIN32WINDOW_HPP
