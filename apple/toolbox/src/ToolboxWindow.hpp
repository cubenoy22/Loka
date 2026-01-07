#ifndef LOKA_TOOLBOX_WINDOW_HPP
#define LOKA_TOOLBOX_WINDOW_HPP

#include "core/Window.hpp"
#include <Windows.h>

class App;
class ToolboxScenePlatformController;

class ToolboxWindow : public Window
{
public:
  ToolboxWindow(PlatformContext *context, const WindowProps &props);
  virtual ~ToolboxWindow();

  void setApp(App *app);
  void ensureSceneMounted();
  void requestInvalidate();
  void flushInvalidate();
  void invalidateWindow();
  void draw();
  WindowPtr window() const { return window_; }

private:
  App *app_;
  WindowPtr window_;
  ToolboxScenePlatformController *scenePlatformController_;
  bool needsInvalidate_;

  void mountScene();
  void teardownScene();
};

#endif // LOKA_TOOLBOX_WINDOW_HPP
