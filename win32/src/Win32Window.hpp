#ifndef LOKA_WIN32WINDOW_HPP
#define LOKA_WIN32WINDOW_HPP

#include "app/core/Window.hpp"
#include <windows.h>
#include <string>

class PlatformContext;
class App;

namespace loka
{
  namespace core
  {
    namespace scene
    {
      class Scene;
    }
  } // namespace core
} // namespace loka

class Win32ScenePlatformController;

/** Win32-specific Window implementation. */
class Win32Window : public Window
{
public:
  Win32Window(PlatformContext *context, const WindowProps &props);
  virtual ~Win32Window();
  virtual Win32Window *asWin32Window()
  {
    return this;
  }

  void setApp(App *app);
  HWND hwnd() const
  {
    return hwnd_;
  }

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  virtual void onShow();
  virtual void onHide();
  virtual bool hasPendingScenePlatformSync() const;
  virtual void synchronizeScenePlatform();

protected:
  HWND hwnd_;
  App *app_;

  virtual void onCreate();

private:
  void createNativeWindow();
  void destroyNativeWindow();
  static void VisibilityChangedThunk(void *userData);
  static void TitleChangedThunk(void *userData);
  static void FrameChangedThunk(void *userData);
  void mountScene();
  void teardownScene();
  bool handleCommand(WPARAM wParam, LPARAM lParam);

  Win32ScenePlatformController *scenePlatformController_;
};

#endif // LOKA_WIN32WINDOW_HPP
