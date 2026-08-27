#include "Win32WideApiTests.hpp"
#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <cwchar>
#include <windows.h>

#include "Win32ScenePlatformController.hpp"
#include "app/RectSurface.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "context/Win32CellContext.hpp"
#include "context/Win32ImageViewContext.hpp"
#include "context/Win32RectSurfaceContext.hpp"
#include "core/State.hpp"

namespace
{
  struct ClassWindowQuery
  {
    explicit ClassWindowQuery(const wchar_t *className)
        : className_(className), hwnd_(0)
    {
    }

    const wchar_t *className_;
    HWND hwnd_;
  };

  BOOL CALLBACK FindClassWindowThunk(HWND hwnd, LPARAM lParam)
  {
    ClassWindowQuery *query = reinterpret_cast<ClassWindowQuery *>(lParam);
    wchar_t className[64];
    const int length = GetClassNameW(hwnd, className, sizeof(className) / sizeof(className[0]));
    if (length > 0 && std::wcscmp(className, query->className_) == 0)
    {
      query->hwnd_ = hwnd;
      return FALSE;
    }
    return TRUE;
  }

  HWND findChildWindowByClass(HWND root, const wchar_t *className)
  {
    ClassWindowQuery query(className);
    EnumChildWindows(root, FindClassWindowThunk, reinterpret_cast<LPARAM>(&query));
    return query.hwnd_;
  }

  void verifyWideChildWindow(HWND root, const wchar_t *className)
  {
    HWND child = findChildWindowByClass(root, className);
    LOKA_VERIFY(child && "the context must materialize its named child window");
    LOKA_VERIFY(IsWindowUnicode(child) &&
                "RegisterClass, CreateWindowEx, and DefWindowProc must use the same wide API family");
  }
} // namespace

void testWin32CustomWindowClassesUseWideApiFamily()
{
  std::printf("\n==== [testWin32CustomWindowClassesUseWideApiFamily] start ====\n");
  HWND root = CreateWindowExW(
      0, L"STATIC", L"wide-api-host", WS_OVERLAPPED, 0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
  assert(root);
  {
    Win32ScenePlatformController controller(root);

    loka::app::CellProps cellProps;
    loka::app::CellNode cellNode(cellProps);
    Win32CellContext cell(&controller, root, 0, 0, 80, 24, &cellNode);
    verifyWideChildWindow(root, L"LOKA_CELL");

    loka::app::ImageViewProps imageProps;
    loka::app::ImageViewNode imageNode(imageProps);
    Win32ImageViewContext image(&controller, root, 0, 30, 80, 60, &imageNode);
    verifyWideChildWindow(root, L"LOKA_IMAGE_VIEW");

    loka::core::MutableState<loka::app::RectSurfaceModel> model((loka::app::RectSurfaceModel()));
    loka::app::RectSurfaceProps surfaceProps;
    surfaceProps.model(&model).size(80, 60);
    loka::app::RectSurfaceNode surfaceNode(surfaceProps);
    Win32RectSurfaceContext surface(&controller, root, 0, 100, 80, 60, &surfaceNode);
    verifyWideChildWindow(root, L"LOKA_RECT_SURFACE");

    cell.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    image.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    surface.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    controller.drainNativeRetirements();
  }
  DestroyWindow(root);
  std::printf("==== [testWin32CustomWindowClassesUseWideApiFamily] PASSED ====\n");
}
