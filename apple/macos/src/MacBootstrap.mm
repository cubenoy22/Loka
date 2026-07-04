#include "app/bootstrap/PlatformBootstrap.hpp"
#include "MacPlatformContext.hpp"
#include "MacProfiler.hpp"

namespace loka
{
  namespace platform
  {
    PlatformContext *CreatePlatformContext()
    {
      return new MacPlatformContext();
    }

    void InitPlatformRuntime()
    {
      InitMacProfiler();
    }
  } // namespace platform
} // namespace loka
