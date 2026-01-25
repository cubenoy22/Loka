#ifndef LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP
#define LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP

#include "app/PlatformContext.hpp"

class ToolboxPlatformContext : public PlatformContext
{
public:
  ToolboxPlatformContext();
  virtual ~ToolboxPlatformContext();

  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;
  virtual Window *createWindow(const WindowProps &props);
  virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *node) const;
};

#endif // LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP
