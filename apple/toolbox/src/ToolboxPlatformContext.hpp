#ifndef LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP
#define LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP

#include "app/PlatformContext.hpp"
#include "loka/core/String.hpp"
#if defined(LOKA_RETRO68)
#include <Files.h>
#endif

class ToolboxPlatformContext : public PlatformContext
{
public:
  ToolboxPlatformContext();
  virtual ~ToolboxPlatformContext();

  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;
  virtual Window *createWindow(const WindowProps &props);
  virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *node) const;
  virtual bool openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const;
  virtual bool createImageFromBlob(const loka::core::resource::Blob &blob,
                                   loka::core::resource::Image &out) const;

#if defined(LOKA_RETRO68)
  static void registerChosenFileSpec(const loka::core::String &displayPath, const FSSpec &spec);
#endif
};

#endif // LOKA_TOOLBOX_PLATFORM_CONTEXT_HPP
