#ifndef LOKA_PLATFORMCONTEXT_HPP
#define LOKA_PLATFORMCONTEXT_HPP

#include <cstddef>

#if defined(_WIN32) || defined(WIN32)
#if !defined(UNICODE) || !defined(_UNICODE)
#error "Loka Win32 targets require UNICODE and _UNICODE"
#endif
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
  /** Reports the largest contiguous allocation the target can currently
      satisfy. Targets without a meaningful answer decline the query. */
  virtual bool queryLargestContiguousAllocation(std::size_t &out) const
  {
    (void)out;
    return false;
  }
  /** Builds an image from `[offset, offset + length)` of `blob`.
      A range rather than the whole buffer, because an LRPK asset is a range
      inside its bag's blob and copying it out to be decoded would put the
      bytes in memory twice -- once in the bag and once per asset drawn.
      Implementations must not read outside the supplied range. */
  virtual bool createImageFromBlob(const loka::core::resource::Blob &blob,
                                   std::size_t offset,
                                   std::size_t length,
                                   loka::core::resource::Image &out) const = 0;
};

#endif // LOKA_PLATFORMCONTEXT_HPP
