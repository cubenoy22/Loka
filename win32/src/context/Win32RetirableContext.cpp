#include "Win32RetirableContext.hpp"

#include <cassert>
#include "../Win32ScenePlatformController.hpp"

Win32RetirableContext::Win32RetirableContext(Win32ScenePlatformController *controller)
    : loka::app::scene::NativeNodeContext(),
      controller_(controller)
{
}

Win32RetirableContext::~Win32RetirableContext() {}

void Win32RetirableContext::positionNativeWindow(HWND hwnd, const loka::win32::NativeRect &geometry)
{
  assert(this->controller_ && "a live HWND must have a native layout owner");
  if (this->controller_)
  {
    this->controller_->positionNativeWindow(hwnd, geometry);
  }
}

void Win32RetirableContext::resizeNativeWindow(HWND hwnd, const loka::win32::NativeRect &geometry)
{
  assert(this->controller_ && "a live HWND must have a native layout owner");
  if (this->controller_)
    this->controller_->resizeNativeWindow(hwnd, geometry);
}

HWND Win32RetirableContext::createNativeChildWindow(DWORD exStyle,
                                                    LPCWSTR className,
                                                    LPCWSTR windowName,
                                                    DWORD style,
                                                    const loka::win32::NativeRect &geometry,
                                                    HWND parent,
                                                    HMENU menu,
                                                    HINSTANCE instance,
                                                    void *createParameter)
{
  assert(this->controller_ && "a native child must have a projection owner");
  return this->controller_
             ? this->controller_->createNativeChildWindow(
                   exStyle, className, windowName, style, geometry, parent, menu, instance, createParameter)
             : 0;
}

void Win32RetirableContext::retireWindow(HWND &hwnd)
{
  if (!hwnd)
  {
    return;
  }
  assert(this->controller_ && "a live HWND must have a controller retirement owner");
  if (!this->controller_)
  {
    return;
  }
  SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
  this->controller_->queueNativeRetirement(hwnd);
  hwnd = NULL;
}
