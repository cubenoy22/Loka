#ifndef DECLARA_MAC_WINDOW_HPP
#define DECLARA_MAC_WINDOW_HPP

#include "core/Window.hpp"

class App;
class MacScenePlatformController;

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

class MacWindow : public Window
{
public:
  MacWindow(PlatformContext *context, const WindowProps &props);
  virtual ~MacWindow();

  void setApp(App *app) { app_ = app; }

  virtual void onShow();
  virtual void onHide();

  void handleWindowWillClose();
  void handleWindowDidResize();

protected:
  virtual void onCreate();

private:
  void createNativeWindow();
  void destroyNativeWindow();
  static void VisibilityChangedThunk(void *userData);
  static void TitleChangedThunk(void *userData);
  void mountScene();
  void teardownScene();

  void *window_;
  void *contentView_;
  void *delegate_;
  App *app_;
  bool closing_;

  MacScenePlatformController *scenePlatformController_;
};

#endif // DECLARA_MAC_WINDOW_HPP
