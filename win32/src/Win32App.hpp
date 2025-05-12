#ifndef DECLARA_WIN32APP_HPP
#define DECLARA_WIN32APP_HPP

#include "core/App.hpp"
#include <windows.h>

class Win32Window;

class Win32App : public App
{
public:
  Win32App(AppBuilder &builder, HINSTANCE hInstance, int nCmdShow);
  ~Win32App() override;
  void run() override;

  // --- Appからのオーバーライド ---
  void quit() override;
  void windowClosed(Window *window) override;

private:
  HINSTANCE hInstance_;
  int nCmdShow_;
  // mainWindow_ は windows[0] として管理するため削除
};

#endif // DECLARA_WIN32APP_HPP
