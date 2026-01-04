#ifndef LOKA_MAC_PLATFORM_CONTEXT_HPP
#define LOKA_MAC_PLATFORM_CONTEXT_HPP

#include "core/PlatformContext.hpp"

class MacPlatformContext : public PlatformContext
{
public:
  MacPlatformContext();
  virtual ~MacPlatformContext();

  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;
  virtual Window *createWindow(const WindowProps &props);
  virtual declara::core::scene::NodeContext *createNodeContext(declara::core::scene::Node *node) const;
};

#endif // LOKA_MAC_PLATFORM_CONTEXT_HPP
