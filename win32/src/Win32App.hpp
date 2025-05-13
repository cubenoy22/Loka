#ifndef DECLARA_WIN32APP_HPP
#define DECLARA_WIN32APP_HPP

#include "core/App.hpp"
#include <windows.h>

class Win32Window;

class Win32App : public App
{
protected:
  Win32App(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow);
  ~Win32App() override;
  friend class Win32PlatformContext;

public:
  void run() override;
  void quit() override;
  void windowClosed(Window *window) override;

private:
  HINSTANCE hInstance_;
  int nCmdShow_;
};

#endif // DECLARA_WIN32APP_HPP
