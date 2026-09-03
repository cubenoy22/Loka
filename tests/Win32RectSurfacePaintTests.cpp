#include "Win32RectSurfacePaintTests.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#include <cstdio>
#include <windows.h>
#include "Win32ScenePlatformController.hpp"
#include "app/RectSurface.hpp"
#include "context/Win32RectSurfaceContext.hpp"
#include "core/State.hpp"
#include "core/resource/Image.hpp"
#include "testing/Win32ScenePlatformTestAccess.hpp"

namespace
{
  void ReleaseRectSurfaceTestBitmap(void *handle, void *)
  {
    if (handle)
    {
      DeleteObject(static_cast<HBITMAP>(handle));
    }
  }

  HBITMAP CreateRectSurfaceTestDib(int width, int height, BYTE *&bits)
  {
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bits = 0;
    return CreateDIBSection(
        0, &info, DIB_RGB_COLORS, reinterpret_cast<void **>(&bits), 0, 0);
  }

  void FillRectSurfaceTestDib(BYTE *bits,
                              int width,
                              int height,
                              BYTE blue,
                              BYTE green,
                              BYTE red,
                              BYTE alpha)
  {
    // DIB-section bits are touched from the CPU only after GDI's batch is
    // flushed, both before rewriting them and before reading them back.
    GdiFlush();
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        BYTE *pixel = bits + (y * width + x) * 4;
        pixel[0] = blue;
        pixel[1] = green;
        pixel[2] = red;
        pixel[3] = alpha;
      }
    }
  }

  // A bottom-up DIB (positive biHeight): memory row 0 is the bottom scanline.
  HBITMAP CreateRectSurfaceTestBottomUpDib(int width, int height, BYTE *&bits)
  {
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bits = 0;
    return CreateDIBSection(
        0, &info, DIB_RGB_COLORS, reinterpret_cast<void **>(&bits), 0, 0);
  }

  void VerifyRectSurfaceTestPixel(const BYTE *bits,
                                  int width,
                                  int x,
                                  int y,
                                  BYTE blue,
                                  BYTE green,
                                  BYTE red)
  {
    GdiFlush();
    const BYTE *pixel = bits + (y * width + x) * 4;
    LOKA_VERIFY(pixel[0] == blue && pixel[1] == green && pixel[2] == red);
  }
} // namespace

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

void testWin32RectSurfaceImagePaintsDdbOpaque()
{
  HDC screen = GetDC(NULL);
  LOKA_VERIFY(screen != NULL);
  HDC target = CreateCompatibleDC(screen);
  HDC source = CreateCompatibleDC(screen);
  LOKA_VERIFY(target != NULL && source != NULL);

  BYTE *targetBits = 0;
  HBITMAP targetBitmap = CreateRectSurfaceTestDib(4, 2, targetBits);
  LOKA_VERIFY(targetBitmap != NULL && targetBits != 0);
  HGDIOBJ oldTarget = SelectObject(target, targetBitmap);
  LOKA_VERIFY(oldTarget != NULL && oldTarget != HGDI_ERROR);

  HBITMAP ddb = CreateCompatibleBitmap(screen, 2, 1);
  LOKA_VERIFY(ddb != NULL);
  DIBSECTION ddbInfo;
  ZeroMemory(&ddbInfo, sizeof(ddbInfo));
  LOKA_VERIFY(GetObjectW(ddb, sizeof(ddbInfo), &ddbInfo) == static_cast<int>(sizeof(BITMAP)));
  LOKA_VERIFY(ddbInfo.dsBm.bmBits == 0);
  HGDIOBJ oldSource = SelectObject(source, ddb);
  LOKA_VERIFY(oldSource != NULL && oldSource != HGDI_ERROR);
  HBRUSH greenBrush = CreateSolidBrush(RGB(0, 255, 0));
  LOKA_VERIFY(greenBrush != NULL);
  RECT sourceRect = {0, 0, 2, 1};
  LOKA_VERIFY(FillRect(source, &sourceRect, greenBrush) != 0);
  DeleteObject(greenBrush);
  SelectObject(source, oldSource);

  {
    const loka::core::resource::Image image = loka::core::resource::Image::FromNative(
        ddb, 2, 1, &ReleaseRectSurfaceTestBitmap, 0);
    LOKA_VERIFY(image.isValid());
    const loka::app::RectSurfaceSprite sprite(loka::app::ImageSprite(0, 0, image));

    FillRectSurfaceTestDib(targetBits, 4, 2, 0, 0, 255, 255);
    RECT equalRect = {1, 0, 3, 1};
    LOKA_VERIFY(DrawRectSurfaceImage(target, equalRect, sprite)
                == loka::app::RECT_SURFACE_PAINT_SUCCEEDED);
    VerifyRectSurfaceTestPixel(targetBits, 4, 0, 0, 0, 0, 255);
    VerifyRectSurfaceTestPixel(targetBits, 4, 1, 0, 0, 255, 0);
    VerifyRectSurfaceTestPixel(targetBits, 4, 2, 0, 0, 255, 0);
    VerifyRectSurfaceTestPixel(targetBits, 4, 3, 0, 0, 0, 255);

    FillRectSurfaceTestDib(targetBits, 4, 2, 0, 0, 255, 255);
    RECT scaledRect = {0, 0, 4, 2};
    LOKA_VERIFY(DrawRectSurfaceImage(target, scaledRect, sprite)
                == loka::app::RECT_SURFACE_PAINT_SUCCEEDED);
    for (int y = 0; y < 2; ++y)
    {
      for (int x = 0; x < 4; ++x)
      {
        VerifyRectSurfaceTestPixel(targetBits, 4, x, y, 0, 255, 0);
      }
    }
  }

  SelectObject(target, oldTarget);
  DeleteObject(targetBitmap);
  DeleteDC(source);
  DeleteDC(target);
  ReleaseDC(NULL, screen);
}

void testWin32RectSurfaceImageKeepsBinaryAlphaPath()
{
  HDC screen = GetDC(NULL);
  LOKA_VERIFY(screen != NULL);
  HDC target = CreateCompatibleDC(screen);
  LOKA_VERIFY(target != NULL);

  BYTE *targetBits = 0;
  HBITMAP targetBitmap = CreateRectSurfaceTestDib(2, 1, targetBits);
  LOKA_VERIFY(targetBitmap != NULL && targetBits != 0);
  HGDIOBJ oldTarget = SelectObject(target, targetBitmap);
  LOKA_VERIFY(oldTarget != NULL && oldTarget != HGDI_ERROR);
  FillRectSurfaceTestDib(targetBits, 2, 1, 0, 0, 255, 255);

  BYTE *sourceBits = 0;
  HBITMAP sourceBitmap = CreateRectSurfaceTestDib(2, 1, sourceBits);
  LOKA_VERIFY(sourceBitmap != NULL && sourceBits != 0);
  FillRectSurfaceTestDib(sourceBits, 2, 1, 0, 255, 0, 0);
  sourceBits[7] = 255;
  {
    const loka::core::resource::Image image = loka::core::resource::Image::FromNative(
        sourceBitmap, 2, 1, &ReleaseRectSurfaceTestBitmap, 0);
    LOKA_VERIFY(image.isValid());
    const loka::app::RectSurfaceSprite sprite(loka::app::ImageSprite(0, 0, image));
    RECT spriteRect = {0, 0, 2, 1};
    LOKA_VERIFY(DrawRectSurfaceImage(target, spriteRect, sprite)
                == loka::app::RECT_SURFACE_PAINT_SUCCEEDED);
  }

  VerifyRectSurfaceTestPixel(targetBits, 2, 0, 0, 0, 0, 255);
  VerifyRectSurfaceTestPixel(targetBits, 2, 1, 0, 0, 255, 0);
  SelectObject(target, oldTarget);
  DeleteObject(targetBitmap);
  DeleteDC(target);
  ReleaseDC(NULL, screen);
}

// A bottom-up 32-bpp DIB taller than the Image's declared height is accepted
// by the storage check; MaskBlt copies the declared rows from the top of the
// stored bitmap, so the mask must be built from those same rows.
void testWin32RectSurfaceImageMasksOversizedBottomUpDibFromTopRows()
{
  HDC screen = GetDC(NULL);
  LOKA_VERIFY(screen != NULL);
  HDC target = CreateCompatibleDC(screen);
  LOKA_VERIFY(target != NULL);

  BYTE *targetBits = 0;
  HBITMAP targetBitmap = CreateRectSurfaceTestDib(2, 1, targetBits);
  LOKA_VERIFY(targetBitmap != NULL && targetBits != 0);
  HGDIOBJ oldTarget = SelectObject(target, targetBitmap);
  LOKA_VERIFY(oldTarget != NULL && oldTarget != HGDI_ERROR);
  FillRectSurfaceTestDib(targetBits, 2, 1, 0, 0, 255, 255);

  // Stored 2x2 bottom-up, declared 2x1: the Image is the top scanline, which
  // is memory row 1. Its alpha is (opaque, transparent); the unused bottom
  // scanline (memory row 0) carries the inverse so a mask built from the
  // wrong rows flips both pixels.
  BYTE *sourceBits = 0;
  HBITMAP sourceBitmap = CreateRectSurfaceTestBottomUpDib(2, 2, sourceBits);
  LOKA_VERIFY(sourceBitmap != NULL && sourceBits != 0);
  FillRectSurfaceTestDib(sourceBits, 2, 2, 0, 255, 0, 0);
  sourceBits[(1 * 2 + 0) * 4 + 3] = 255;
  sourceBits[(0 * 2 + 1) * 4 + 3] = 255;
  {
    const loka::core::resource::Image image = loka::core::resource::Image::FromNative(
        sourceBitmap, 2, 1, &ReleaseRectSurfaceTestBitmap, 0);
    LOKA_VERIFY(image.isValid());
    const loka::app::RectSurfaceSprite sprite(loka::app::ImageSprite(0, 0, image));
    RECT spriteRect = {0, 0, 2, 1};
    LOKA_VERIFY(DrawRectSurfaceImage(target, spriteRect, sprite)
                == loka::app::RECT_SURFACE_PAINT_SUCCEEDED);
  }

  VerifyRectSurfaceTestPixel(targetBits, 2, 0, 0, 0, 255, 0);
  VerifyRectSurfaceTestPixel(targetBits, 2, 1, 0, 0, 0, 255);
  SelectObject(target, oldTarget);
  DeleteObject(targetBitmap);
  DeleteDC(target);
  ReleaseDC(NULL, screen);
}
