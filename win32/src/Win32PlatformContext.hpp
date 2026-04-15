#ifndef LOKA_WIN32PLATFORMCONTEXT_HPP
#define LOKA_WIN32PLATFORMCONTEXT_HPP
#include "app/PlatformContext.hpp"
#include "app/core/Window.hpp"
#include "Win32App.hpp"

// 前方宣言（Win32Windowは別途定義）
class Win32Window;
class AppConfigurable;

class Win32PlatformContext : public PlatformContext
{
public:
  Win32PlatformContext();
  ~Win32PlatformContext();
  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const;

  virtual Window *createWindow(const WindowProps &props);

  virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *node) const;
  virtual bool openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const;
  virtual bool createImageFromBlob(const loka::core::resource::Blob &blob,
                                   loka::core::resource::Image &out) const;
};

#endif // LOKA_WIN32PLATFORMCONTEXT_HPP
