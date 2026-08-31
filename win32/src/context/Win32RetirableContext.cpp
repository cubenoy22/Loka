#include "Win32RetirableContext.hpp"

#include <cassert>
#include "../Win32ScenePlatformController.hpp"

Win32RetirableContext::Win32RetirableContext(Win32ScenePlatformController *controller)
    : loka::app::scene::NativeNodeContext(),
      controller_(controller)
{
}

Win32RetirableContext::~Win32RetirableContext() {}

void Win32RetirableContext::positionNativeWindow(HWND hwnd,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height)
{
  assert(this->controller_ && "a live HWND must have a native layout owner");
  if (this->controller_)
  {
    this->controller_->positionNativeWindow(hwnd, x, y, width, height);
  }
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
