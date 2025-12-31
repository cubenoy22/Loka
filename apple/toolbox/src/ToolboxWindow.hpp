#ifndef DECLARA_TOOLBOX_WINDOW_HPP
#define DECLARA_TOOLBOX_WINDOW_HPP

#include "core/Window.hpp"
#include <Windows.h>

class App;

class ToolboxWindow : public Window
{
public:
  ToolboxWindow(PlatformContext *context,
                declara::core::scene::Scene *initialScene,
                const WindowOptions &opts);
  virtual ~ToolboxWindow();

  void setApp(App *app);
  void invalidateWindow();
  void draw();
  WindowPtr window() const { return window_; }

private:
  App *app_;
  WindowPtr window_;
};

#endif // DECLARA_TOOLBOX_WINDOW_HPP
