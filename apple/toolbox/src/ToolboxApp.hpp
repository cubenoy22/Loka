#ifndef LOKA_TOOLBOX_APP_HPP
#define LOKA_TOOLBOX_APP_HPP

#include "core/App.hpp"

class ToolboxApp : public App
{
protected:
  explicit ToolboxApp(AppConfigurable *config);
  virtual ~ToolboxApp();
  friend class ToolboxPlatformContext;

public:
  virtual void run();
  virtual void quit();
  virtual void windowClosed(Window *window);

private:
  bool running_;
};

#endif // LOKA_TOOLBOX_APP_HPP
