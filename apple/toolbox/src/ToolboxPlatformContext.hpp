#ifndef LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP
#define LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP

#include "core/PlatformContext.hpp"

class ToolboxPlatformContext : public PlatformContext
{
public:
  ToolboxPlatformContext();
  virtual ~ToolboxPlatformContext();

  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;
  virtual Window *createWindow(const WindowProps &props);
  virtual declara::core::scene::NodeContext *createNodeContext(declara::core::scene::Node *node) const;
};

#endif // LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP
