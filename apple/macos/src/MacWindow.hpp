#ifndef LOKA_MAC_WINDOW_HPP
#define LOKA_MAC_WINDOW_HPP

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
  virtual MacWindow *asMacWindow() { return this; }

  void setApp(App *app);

  virtual void onShow();
  virtual void onHide();

  void handleWindowWillClose();
  void handleWindowDidResize();
  void handleWindowDidBecomeKey();

protected:
  virtual void onCreate();

private:
  void createNativeWindow();
  void destroyNativeWindow();
  static void VisibilityChangedThunk(void *userData);
  static void TitleChangedThunk(void *userData);
  static void FrameChangedThunk(void *userData);
  void mountScene();
  void teardownScene();

  void *window_;
  void *contentView_;
  void *delegate_;
  App *app_;
  bool closing_;

  MacScenePlatformController *scenePlatformController_;
};

#endif // LOKA_MAC_WINDOW_HPP
