#ifndef LOKA_WIN32WINDOW_HPP
#define LOKA_WIN32WINDOW_HPP

#include "core/Window.hpp"
#include <windows.h>
#include <string>

class PlatformContext;
class App;

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class Scene;
    }
  }
}

class Win32ScenePlatformController;

// 🦊 Win32固有のWindow実装
class Win32Window : public Window
{
public:
  Win32Window(PlatformContext *context, const WindowProps &props);
  virtual ~Win32Window();

  // Appの参照を設定するメソッド（アプリケーションのライフサイクル管理用）
  void setApp(App *app);
  HWND hwnd() const { return hwnd_; }

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  // --- Windowのイベントハンドラを明示的にoverride ---
  virtual void onShow();
  virtual void onHide();

protected:
  HWND hwnd_;
  App *app_; // アプリケーションインスタンスの参照

  virtual void onCreate();

private:
  void createNativeWindow();
  void destroyNativeWindow();
  static void VisibilityChangedThunk(void *userData);
  static void TitleChangedThunk(void *userData);
  void mountScene();
  void teardownScene();
  bool handleCommand(WPARAM wParam, LPARAM lParam);

  Win32ScenePlatformController *scenePlatformController_;
};

#endif // LOKA_WIN32WINDOW_HPP
