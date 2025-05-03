#ifndef DECLARA_EMPTYCONTEXT_HPP
#define DECLARA_EMPTYCONTEXT_HPP

#include "Renderer.hpp"
#include "PlatformContext.hpp"
#include "Window.hpp"

// EmptyRenderer: 何もしないRenderer 実装
class EmptyRenderer : public Renderer
{
public:
  static EmptyRenderer &instance()
  {
    static EmptyRenderer inst;
    return inst;
  }
  void processEvents() {}
  void createText(const std::string &) {}
  void createTextInput(void *) {}
  void createButton(const std::string &, bool, void (*)()) {}
  void setButtonEnabled(const std::string &, bool) {}

private:
  EmptyRenderer() {}
};

// EmptyWindowContext: 何もしないWindowContext 実装
class EmptyWindowContext : public WindowContext
{
public:
  static EmptyWindowContext &instance()
  {
    static EmptyWindowContext inst;
    return inst;
  }
  Window *getWindow() { return 0; }

private:
  EmptyWindowContext() {}
};

// EmptyPlatformContext: EmptyRenderer/EmptyWindowContextを持つPlatformContext
class EmptyPlatformContext : public PlatformContext
{
public:
  static EmptyPlatformContext &instance()
  {
    static EmptyPlatformContext inst;
    return inst;
  }
  EmptyPlatformContext()
      : PlatformContext(&EmptyRenderer::instance(), &EmptyWindowContext::instance()) {}
};

#endif // DECLARA_EMPTYCONTEXT_HPP
