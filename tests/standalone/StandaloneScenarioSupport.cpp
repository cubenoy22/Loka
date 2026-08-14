#include "StandaloneScenarioSupport.hpp"

#include "app/core/Window.hpp"
#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"

namespace loka
{
  namespace standalone_tests
  {
    scenario_tests::CaptureContentBounds StandaloneContentBounds(Window *window)
    {
      scenario_tests::CaptureContentBounds result;
      if (!window)
      {
        return result;
      }
      const core::Frame frame = window->frameState().get();
      if (frame.width <= 0 || frame.height <= 0)
      {
        return result;
      }
      result.available = true;
      result.right = frame.width;
      result.bottom = frame.height;
      return result;
    }

    platform::file::FileHandle ResolveStandaloneAuditFile()
    {
      platform::file::FileHandle result;
      if (!platform::file::ResolveApplicationSidecar(file::File::Application() << file::File("LOG.TXT"), result))
      {
        return platform::file::FileHandle();
      }
      return result;
    }
  } // namespace standalone_tests
} // namespace loka
