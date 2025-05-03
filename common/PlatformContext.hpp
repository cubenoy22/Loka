#ifndef DECLARA_PLATFORMCONTEXT_HPP
#define DECLARA_PLATFORMCONTEXT_HPP

#include "Renderer.hpp"
#include "Window.hpp"

class WindowContext {
public:
  virtual ~WindowContext() {}
  virtual Window *getWindow() = 0;
};

class PlatformContext
{
public:
  Renderer *renderer;
  PlatformContext(Renderer *r, WindowContext *wctx) : renderer(r), windowContext_(wctx) {}
  const WindowContext *windowContext() const { return windowContext_; }
private:
  WindowContext *windowContext_;
};

#endif // DECLARA_PLATFORMCONTEXT_HPP
