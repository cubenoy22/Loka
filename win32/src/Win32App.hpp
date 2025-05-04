#ifndef DECLARA_WIN32APP_HPP
#define DECLARA_WIN32APP_HPP

#include "App.hpp"
#include <windows.h>
#include "Win32Window.hpp"

class Win32App : public App
{
public:
  Win32App(HINSTANCE hInstance, int nCmdShow);
  void run();

private:
  HINSTANCE hInstance_;
  int nCmdShow_;
  Win32Window *mainWindow_;
  // 必要に応じてHWNDやWindow管理メンバを追加
};

#endif // DECLARA_WIN32APP_HPP
