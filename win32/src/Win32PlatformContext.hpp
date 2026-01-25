#ifndef LOKA_WIN32PLATFORMCONTEXT_HPP
#define LOKA_WIN32PLATFORMCONTEXT_HPP
#include "app/PlatformContext.hpp"
#include "app/Window.hpp"
#include "Win32App.hpp"

// 前方宣言（Win32Windowは別途定義）
class Win32Window;
class AppConfigurable;

namespace loka
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

  virtual loka::core::scene::NodeContext *createNodeContext(loka::core::scene::Node *node) const;
};

#endif // LOKA_WIN32PLATFORMCONTEXT_HPP
