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
        struct PendingInvalidationSnapshot
        {
          PendingInvalidationSnapshot()
              : hwnd(0),
                eraseBackground(FALSE),
                fullWindow(false),
                includeChildren(false)
          {
            rect.left = rect.top = rect.right = rect.bottom = 0;
          }

          HWND hwnd;
          RECT rect;
          BOOL eraseBackground;
          bool fullWindow;
          bool includeChildren;
        };

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

        static ::loka::app::scene::NodeDirtyFlags lastOnChangeFlags(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.lastOnChangeFlags;
        }

        static bool lastOnChangeRequiredLayout(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.lastOnChangeRequiredLayout;
        }

        static bool lastOnChangeFullRebuild(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.lastOnChangeFullRebuild;
        }

        static int queuedFullWindowInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedFullWindowInvalidates;
        }

        static int queuedRectInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedRectInvalidates;
        }

        static int queuedLayoutBoundsInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedLayoutBoundsInvalidates;
        }

        static int queuedPaintBoundsInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedPaintBoundsInvalidates;
        }

        static int queuedMissingBoundsInvalidates(const ::Win32ScenePlatformController &controller)
        {
          return controller.redrawStats_.queuedMissingBoundsInvalidates;
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
          return queuedCompositedInvalidates(controller) + queuedOpaquePaintInvalidates(controller)
                 + queuedGenericPaintInvalidates(controller);
        }

        static int clientWidth(const ::Win32ScenePlatformController &controller)
        {
          return controller.clientWidth_;
        }

        static int clientHeight(const ::Win32ScenePlatformController &controller)
        {
          return controller.clientHeight_;
        }

        static bool queryPendingInvalidation(const ::Win32ScenePlatformController &controller,
                                             std::size_t index,
                                             PendingInvalidationSnapshot &out)
        {
          if (index >= controller.pendingInvalidations_.size())
          {
            return false;
          }
          const ::Win32ScenePlatformController::PendingInvalidate &entry = controller.pendingInvalidations_[index];
          out.hwnd = entry.hwnd;
          out.rect = entry.rect;
          out.eraseBackground = entry.eraseBackground;
          out.fullWindow = entry.fullWindow;
          out.includeChildren = entry.includeChildren;
          return true;
        }
      };
    } // namespace testing
  } // namespace dsl
} // namespace loka

#endif // LOKA_WIN32_SCENE_PLATFORM_TEST_ACCESS_HPP
