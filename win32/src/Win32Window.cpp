#include "Win32Window.hpp"
#include <windows.h>

Win32Window::Win32Window(Renderer *renderer, HWND hwnd)
    : Window(renderer), hwnd_(hwnd)
{
  // visibilityプロパティの変更を監視
  if (visibility)
  {
    // C++98: static関数＋thisポインタ渡しでコールバック
    visibility->bind(&Win32Window::VisibilityChangedThunk, this, true, false);
  }
}

// static thunk for BindableProp<bool>::OnChangeFn
void Win32Window::VisibilityChangedThunk(bool visible, void *userData)
{
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (self)
    self->onVisibilityChanged(visible);
}

void Win32Window::onVisibilityChanged(bool visible)
{
  if (visible)
  {
    if (!hwnd_)
    {
      createNativeWindow();
    }
    else
    {
      ShowWindow(hwnd_, SW_SHOW);
    }
  }
  else
  {
    if (hwnd_)
    {
      destroyNativeWindow();
    }
  }
}

void Win32Window::createNativeWindow()
{
  // 空実装
}

void Win32Window::destroyNativeWindow()
{
  // 空実装
}
