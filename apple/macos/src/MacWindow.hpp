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
      class StaticNodeManager;
    }
  }
}

class MacWindow : public Window
{
public:
  MacWindow(PlatformContext *context, declara::core::scene::Scene *initialScene, const WindowOptions &opts);
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

  declara::core::scene::StaticNodeManager *nodeManager_;
  MacScenePlatformController *scenePlatformController_;
};

#endif // DECLARA_MAC_WINDOW_HPP
