#ifndef LOKA_PLATFORMCONTEXT_HPP
#define LOKA_PLATFORMCONTEXT_HPP
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#else
// Provide a placeholder type for non-Windows builds
typedef void *HINSTANCE;
#endif

class AppConfigurable;
class PlatformContext;
class App;
class Window;
struct WindowProps;

namespace loka
{
  namespace file
  {
    struct File;
  }

  namespace platform
  {
    namespace file
    {
      struct FileHandle;
    }
  } // namespace platform

  namespace core
  {
    namespace resource
    {
      class Blob;
      class Image;
    } // namespace resource
  } // namespace core

  namespace app
  {
    namespace scene
    {
      class Scene;
      class Node;
      struct NodeContext;
    } // namespace scene
  } // namespace app
} // namespace loka

class PlatformContext
{
public:
  virtual ~PlatformContext() {}

  // Creates the platform-specific App instance.
  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const = 0;

  // Creates a platform-specific Window. Implementations should assert on invalid owner state.
  virtual Window *createWindow(const WindowProps &props) = 0;

  virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *node) const = 0;
  virtual bool openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const = 0;
  virtual bool createImageFromBlob(const loka::core::resource::Blob &blob, loka::core::resource::Image &out) const = 0;
};

#endif // LOKA_PLATFORMCONTEXT_HPP
