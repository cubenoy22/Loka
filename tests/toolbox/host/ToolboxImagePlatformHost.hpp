#ifndef LOKA_TEST_TOOLBOX_IMAGE_PLATFORM_HOST_HPP
#define LOKA_TEST_TOOLBOX_IMAGE_PLATFORM_HOST_HPP

// Replace unrelated native app/window factories while compiling the complete
// production ToolboxPlatformContext translation unit, including image decode.
#define LOKA_TOOLBOX_APP_HPP
#define LOKA_TOOLBOX_WINDOW_HPP
#include "app/core/App.hpp"
#include "app/core/Window.hpp"
class ToolboxApp : public App
{
public:
  explicit ToolboxApp(AppConfigurable *config) : App(config) {}
  virtual void quit() {}
};
class ToolboxWindow : public Window
{
public:
  ToolboxWindow(PlatformContext *context, const WindowProps &props)
      : Window(context, props) {}
};
#endif
