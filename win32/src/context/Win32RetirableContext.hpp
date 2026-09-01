#ifndef LOKA_WIN32_RETIRABLE_CONTEXT_HPP
#define LOKA_WIN32_RETIRABLE_CONTEXT_HPP

#include <windows.h>
#include "app/scene/projection/NativeNodeContext.hpp"

class Win32ScenePlatformController;

/** Platform-context base that transfers an HWND to its controller at the
    terminal lifecycle fact. The context dies synchronously; the HWND does
    not cross its native destruction line until the App safe point. */
class Win32RetirableContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit Win32RetirableContext(Win32ScenePlatformController *controller);
  virtual ~Win32RetirableContext();

protected:
  void retireWindow(HWND &hwnd);
  void positionNativeWindow(HWND hwnd, int x, int y, int width, int height);

  Win32ScenePlatformController *controller() const
  {
    return this->controller_;
  }

private:
  Win32ScenePlatformController *controller_;
};

#endif // LOKA_WIN32_RETIRABLE_CONTEXT_HPP
