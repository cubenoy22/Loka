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

// #597: a model-only update queues the complete child without erasing.
void testWin32RectSurfacePaintQueuesBoundedParentSubtree()
{
  std::printf("\n==== [testWin32RectSurfacePaintQueuesBoundedParentSubtree] start ====\n");
  HWND root = CreateWindowExW(
      0, L"STATIC", L"rect-surface-paint-host", WS_OVERLAPPED, 0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
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
    LOKA_VERIFY(invalidation.hwnd == FindWindowExW(root, NULL, L"LOKA_RECT_SURFACE", NULL));
    LOKA_VERIFY(invalidation.fullWindow);
    LOKA_VERIFY(!invalidation.includeChildren);
    LOKA_VERIFY(invalidation.eraseBackground == FALSE);
    context.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    controller.drainNativeRetirements();
  }
  DestroyWindow(root);
  std::printf("==== [testWin32RectSurfacePaintQueuesBoundedParentSubtree] PASSED ====\n");
}
