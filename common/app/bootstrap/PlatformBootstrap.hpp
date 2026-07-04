#ifndef LOKA_APP_BOOTSTRAP_PLATFORMBOOTSTRAP_HPP
#define LOKA_APP_BOOTSTRAP_PLATFORMBOOTSTRAP_HPP

class PlatformContext;

namespace loka
{
  namespace platform
  {
    PlatformContext *CreatePlatformContext();
    void InitPlatformRuntime();
  } // namespace platform
} // namespace loka

#endif // LOKA_APP_BOOTSTRAP_PLATFORMBOOTSTRAP_HPP
