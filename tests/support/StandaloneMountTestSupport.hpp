#ifndef LOKA_TESTS_SUPPORT_STANDALONE_MOUNT_TEST_SUPPORT_HPP
#define LOKA_TESTS_SUPPORT_STANDALONE_MOUNT_TEST_SUPPORT_HPP

#include "app/PlatformContext.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace testing
  {
    /** Minimal host context for driving TEST-only standalone Window idle
        callbacks without projecting a successful Scene. */
    class StandaloneMountTestPlatformContext : public PlatformContext
    {
    public:
      virtual App *createApp(AppConfigurable *, HINSTANCE, int) const
      {
        return 0;
      }

      virtual Window *createWindow(const WindowProps &props)
      {
        return new Window(this, props);
      }

      virtual app::scene::NodeContext *createNodeContext(app::scene::Node *) const
      {
        return 0;
      }

      virtual bool openFile(const file::File &, platform::file::FileHandle &) const
      {
        return false;
      }

      virtual bool
      createImageFromBlob(const core::resource::Blob &, std::size_t, std::size_t, core::resource::Image &) const
      {
        return false;
      }
    };
  } // namespace testing
} // namespace loka

#endif // LOKA_TESTS_SUPPORT_STANDALONE_MOUNT_TEST_SUPPORT_HPP
