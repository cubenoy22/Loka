#include "app/bootstrap/PlatformBootstrap.hpp"
#include "Win32PlatformContext.hpp"
#include "platform/Win32Profiler.hpp"

namespace loka
{
  namespace platform
  {
    PlatformContext *CreatePlatformContext()
    {
      return new Win32PlatformContext();
    }

    void InitPlatformRuntime()
    {
      InitWin32Profiler();
    }
  } // namespace platform
} // namespace loka
