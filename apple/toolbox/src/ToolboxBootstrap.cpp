#include "app/bootstrap/PlatformBootstrap.hpp"
#include "ToolboxPlatformContext.hpp"
#include "ToolboxProfiler.hpp"

namespace loka
{
  namespace platform
  {
    PlatformContext *CreatePlatformContext()
    {
      return new ToolboxPlatformContext();
    }

    void InitPlatformRuntime()
    {
      InitToolboxProfiler();
    }
  } // namespace platform
} // namespace loka
