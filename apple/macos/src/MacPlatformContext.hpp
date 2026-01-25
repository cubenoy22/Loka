#ifndef LOKA_MAC_PLATFORM_CONTEXT_HPP
#define LOKA_MAC_PLATFORM_CONTEXT_HPP

#include "app/PlatformContext.hpp"

class MacPlatformContext : public PlatformContext
{
public:
  MacPlatformContext();
  virtual ~MacPlatformContext();

  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;
  virtual Window *createWindow(const WindowProps &props);
  virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *node) const;
};

#endif // LOKA_MAC_PLATFORM_CONTEXT_HPP
