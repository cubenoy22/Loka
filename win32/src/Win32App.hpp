#ifndef DECLARA_WIN32APP_HPP
#define DECLARA_WIN32APP_HPP

#include "core/App.hpp"
#include <windows.h>
// #include "Win32Window.hpp" // Win32Window.hpp から Win32App を前方宣言したので、ここでは不要かも (循環参照防止)
class Win32Window; // 前方宣言で十分

class Win32App : public App
{
public:
  Win32App(HINSTANCE hInstance, int nCmdShow);
  ~Win32App() override; // override を追加
  void run() override;  // override を追加

  // --- Appからのオーバーライド ---
  void quit() override;                       // <-- 再度追加
  void windowClosed(Window *window) override; // <-- 再度追加
  // addWindow は必要ならオーバーライド

private:
  HINSTANCE hInstance_;
  int nCmdShow_;
  // mainWindow_ は windows[0] として管理するため削除
};

#endif // DECLARA_WIN32APP_HPP
