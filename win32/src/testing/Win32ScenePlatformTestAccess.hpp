#ifndef LOKA_WIN32_SCENE_PLATFORM_TEST_ACCESS_HPP
#define LOKA_WIN32_SCENE_PLATFORM_TEST_ACCESS_HPP

#include "../Win32ScenePlatformController.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class Win32ScenePlatformTestAccess
      {
      public:
        static void resetRedrawStats(::Win32ScenePlatformController &controller)
        {
          controller.redrawStats_.reset();
        }

        static int onChangeCalls(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.onChangeCalls;
        }

        static int onBoundaryApplyCalls(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.onBoundaryApplyCalls;
        }

        static int queuedFullWindowInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedFullWindowInvalidates;
        }

        static int queuedRectInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedRectInvalidates;
        }

        static int queuedCompositedInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedCompositedInvalidates;
        }

        static int queuedOpaquePaintInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedOpaquePaintInvalidates;
        }

        static int queuedGenericPaintInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedGenericPaintInvalidates;
        }

        static int queuedPaintInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return queuedCompositedInvalidates(controller) +
                 queuedOpaquePaintInvalidates(controller) +
                 queuedGenericPaintInvalidates(controller);
        }

        static int clientWidth(const ::Win32ScenePlatformController &controller)
        {
          return controller.clientWidth_;
        }

        static int clientHeight(const ::Win32ScenePlatformController &controller)
        {
          return controller.clientHeight_;
        }
      };
    } // namespace testing
  } // namespace dsl
} // namespace loka

#endif // LOKA_WIN32_SCENE_PLATFORM_TEST_ACCESS_HPP
