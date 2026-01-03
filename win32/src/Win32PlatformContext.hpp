#ifndef DECLARA_WIN32PLATFORMCONTEXT_HPP
#define DECLARA_WIN32PLATFORMCONTEXT_HPP
#include "core/PlatformContext.hpp"
#include "core/Window.hpp"
#include "Win32App.hpp"

// 前方宣言（Win32Windowは別途定義）
class Win32Window;
class AppConfigurable;

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class Scene;
    }
  }
}

class Win32PlatformContext : public PlatformContext
{
public:
  Win32PlatformContext();
  ~Win32PlatformContext();
  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;

  virtual Window *createWindow(const WindowProps &props);

  virtual declara::core::scene::NodeContext *createNodeContext(declara::core::scene::Node *node) const;
};

#endif // DECLARA_WIN32PLATFORMCONTEXT_HPP
