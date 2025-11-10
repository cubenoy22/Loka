#ifndef DECLARA_WIN32APP_HPP
#define DECLARA_WIN32APP_HPP

#include "core/App.hpp"
#include <windows.h>

class Win32Window;

class Win32App : public App
{
protected:
  Win32App(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow);
  virtual ~Win32App();
  friend class Win32PlatformContext;

public:
  virtual void run();
  virtual void quit();
  virtual void windowClosed(Window *window);

private:
  HINSTANCE hInstance_;
  int nCmdShow_;
};

#endif // DECLARA_WIN32APP_HPP
