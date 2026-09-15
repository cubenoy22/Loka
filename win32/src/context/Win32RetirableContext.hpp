#ifndef LOKA_WIN32_RETIRABLE_CONTEXT_HPP
#define LOKA_WIN32_RETIRABLE_CONTEXT_HPP

#include <windows.h>
#include "app/scene/projection/NativeNodeContext.hpp"

class Win32ScenePlatformController;

/** Platform-context base that transfers an HWND to its controller at the
    terminal lifecycle fact. The context dies synchronously; the HWND does
    not cross its native destruction line until the App safe point. */
class Win32RetirableContext : public loka::app::scene::NativeNodeContext,
                              public loka::app::scene::IBoundaryTaggedContext
{
public:
  explicit Win32RetirableContext(Win32ScenePlatformController *controller);
  virtual ~Win32RetirableContext();
  // Rail writes must settle the owning Boundary's derived states; the Window
  // tracker is a different tracker and cannot settle this Boundary.
  virtual loka::app::scene::IBoundaryTaggedContext *asBoundaryTagged()
  {
    return this;
  }
  virtual void setBoundary(loka::app::scene::BoundaryNode *boundary)
  {
    this->boundary_ = boundary;
  }
  virtual loka::app::scene::BoundaryNode *boundary() const
  {
    return this->boundary_;
  }
  /** EXACT damage uses device pixels in this HWND's client coordinates.
      Only contexts that return non-empty EXACT need to expose a target. */
  virtual HWND paintHwnd() const
  {
    return 0;
  }
  /** A visit-local coordinate convention, not a retained HWND identity.
      Each answer record's context supplies its own native destination. */
  static loka::app::scene::PaintScope paintScope()
  {
    const loka::app::scene::PaintScope scope = {1, 0, 0, 0, 0, 0, 0};
    return scope;
  }

protected:
  void retireWindow(HWND &hwnd);
  void positionNativeWindow(HWND hwnd, int x, int y, int width, int height);
  HWND createNativeChildWindow(DWORD exStyle,
                               LPCWSTR className,
                               LPCWSTR windowName,
                               DWORD style,
                               int x,
                               int y,
                               int width,
                               int height,
                               HWND parent,
                               HMENU menu,
                               HINSTANCE instance,
                               void *createParameter);

  Win32ScenePlatformController *controller() const
  {
    return this->controller_;
  }

private:
  loka::app::scene::BoundaryNode *boundary_;
  Win32ScenePlatformController *controller_;
};

#endif // LOKA_WIN32_RETIRABLE_CONTEXT_HPP
