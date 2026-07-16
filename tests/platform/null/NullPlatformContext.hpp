#ifndef LOKA_TESTS_PLATFORM_NULL_PLATFORM_CONTEXT_HPP
#define LOKA_TESTS_PLATFORM_NULL_PLATFORM_CONTEXT_HPP

#include "app/PlatformContext.hpp"

class NullPlatformContext : public PlatformContext
{
public:
  NullPlatformContext();
  virtual ~NullPlatformContext();

  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;
  virtual Window *createWindow(const WindowProps &props);
  virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *node) const;
  virtual bool openFile(const loka::file::File &item,
                        loka::platform::file::FileHandle &out) const;
  virtual bool createImageFromBlob(const loka::core::resource::Blob &blob,
                                   loka::core::resource::Image &out) const;
};

#endif // LOKA_TESTS_PLATFORM_NULL_PLATFORM_CONTEXT_HPP
