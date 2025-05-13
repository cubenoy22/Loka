#ifndef DECLARA_WIN32WINDOW_HPP
#define DECLARA_WIN32WINDOW_HPP

#include "core/Window.hpp"
#include <windows.h>
#include <string>

class PlatformContext;
class App;

// 🦊 Win32固有のWindow実装
class Win32Window : public Window
{
public:
  // WindowOptionsごと受け取る形に変更
  Win32Window(PlatformContext *context, const WindowOptions &opts);

  // Appの参照を設定するメソッド（アプリケーションのライフサイクル管理用）
  void setApp(App *app) { app_ = app; }

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  // --- Windowのイベントハンドラを明示的にoverride ---
  void onShow() override;
  void onHide() override;

protected:
  HWND hwnd_;
  HWND buttonHwnd_; // --- 追加: ボタン用HWND
  App *app_;        // アプリケーションインスタンスの参照

  void onCreate() override;

private:
  void createNativeWindow();
  void destroyNativeWindow();
  static void VisibilityChangedThunk(void *userData);
  static void TitleChangedThunk(void *userData);
};

#endif // DECLARA_WIN32WINDOW_HPP
