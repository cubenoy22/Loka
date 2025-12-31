#ifndef DECLARA_TOOLBOX_PLATFORM_CONTEXT_HPP
#define DECLARA_TOOLBOX_PLATFORM_CONTEXT_HPP

#include "core/PlatformContext.hpp"

class ToolboxPlatformContext : public PlatformContext
{
public:
  ToolboxPlatformContext();
  virtual ~ToolboxPlatformContext();

  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;
  virtual Window *createWindow(declara::core::scene::Scene *initialScene, const WindowOptions &opts);
  virtual declara::core::scene::NodeContext *createNodeContext(declara::core::scene::Node *node) const;
};

#endif // DECLARA_TOOLBOX_PLATFORM_CONTEXT_HPP
