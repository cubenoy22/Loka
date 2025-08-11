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
  App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const override;

  Window *createWindow(declara::core::scene::Scene *initialScene, const WindowOptions &opts) override;
};

#endif // DECLARA_WIN32PLATFORMCONTEXT_HPP
