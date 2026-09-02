#include "Win32RectSurfacePaintTests.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#include <cstdio>
#include <windows.h>
#include "Win32ScenePlatformController.hpp"
#include "app/RectSurface.hpp"
#include "context/Win32RectSurfaceContext.hpp"
#include "core/State.hpp"
#include "testing/Win32ScenePlatformTestAccess.hpp"

// #293: the first text-page projection paints its overlapping Text correctly,
// then the RectSurface model's older child-only invalidation flushes last and
// covers it. The surface must queue its bounded parent subtree so native
// sibling order is replayed without invalidating the whole window.
void testWin32RectSurfacePaintQueuesBoundedParentSubtree()
{
  std::printf("\n==== [testWin32RectSurfacePaintQueuesBoundedParentSubtree] start ====\n");
  HWND root = CreateWindowExW(
      0, L"STATIC", L"rect-surface-paint-host", WS_OVERLAPPED, 0, 0, 320, 240, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(root);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96));
    loka::core::MutableState<loka::app::RectSurfaceModel> model((loka::app::RectSurfaceModel()));
    loka::app::RectSurfaceProps props;
    props.model(&model).size(100, 60);
    loka::app::RectSurfaceNode node(props);
    Win32RectSurfaceContext context(&controller, root, 10, 20, 100, 60, &node);

    typedef loka::dsl::testing::Win32ScenePlatformTestAccess Access;
    Access::PendingInvalidationSnapshot invalidation;
    LOKA_VERIFY(Access::queryPendingInvalidation(controller, 0, invalidation));
    LOKA_VERIFY(!Access::queryPendingInvalidation(controller, 1, invalidation));
    LOKA_VERIFY(invalidation.hwnd == root);
    LOKA_VERIFY(!invalidation.fullWindow);
    LOKA_VERIFY(invalidation.includeChildren);
    LOKA_VERIFY(invalidation.eraseBackground != FALSE);
    LOKA_VERIFY(invalidation.rect.left == 10 && invalidation.rect.top == 20 && invalidation.rect.right == 110
                && invalidation.rect.bottom == 80);
    context.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    controller.drainNativeRetirements();
  }
  DestroyWindow(root);
  std::printf("==== [testWin32RectSurfacePaintQueuesBoundedParentSubtree] PASSED ====\n");
}
