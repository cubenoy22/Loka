#include "Win32RectSurfaceContext.hpp"
#include <cassert>
#include "../Win32ScenePlatformController.hpp"
#include "app/RectSurface.hpp"

namespace
{
  const wchar_t kRectSurfaceClassName[] = L"LOKA_RECT_SURFACE";
  const COLORREF kRectSurfaceClearColor = RGB(255, 255, 255);
  // Ternary raster op "D": leave the destination pixel untouched (wingdi.h has no name for it).
  const DWORD kRopKeepDestination = 0x00AA0029;

  bool SelectRectSurfaceBitmap(HDC dc, HBITMAP bitmap, HGDIOBJ &oldObject)
  {
    oldObject = SelectObject(dc, bitmap);
    return oldObject != 0 && oldObject != HGDI_ERROR;
  }

  void DrawScaledRectSurfaceImage(HDC hdc,
                                  const RECT &spriteRect,
                                  HBITMAP bitmap,
                                  HBITMAP mask,
                                  int width,
                                  int height)
  {
    const int projectedWidth = spriteRect.right - spriteRect.left;
    const int projectedHeight = spriteRect.bottom - spriteRect.top;
    if (projectedWidth <= 0 || projectedHeight <= 0)
    {
      return;
    }

    BITMAPINFO sourceInfo;
    ZeroMemory(&sourceInfo, sizeof(sourceInfo));
    sourceInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    sourceInfo.bmiHeader.biWidth = projectedWidth;
    sourceInfo.bmiHeader.biHeight = -projectedHeight;
    sourceInfo.bmiHeader.biPlanes = 1;
    sourceInfo.bmiHeader.biBitCount = 32;
    sourceInfo.bmiHeader.biCompression = BI_RGB;
    void *projectedSourceBits = 0;
    HBITMAP projectedSource = CreateDIBSection(
        0, &sourceInfo, DIB_RGB_COLORS, &projectedSourceBits, 0, 0);

    struct MonochromeBitmapInfo
    {
      BITMAPINFOHEADER header;
      RGBQUAD colors[2];
    } maskInfo;
    ZeroMemory(&maskInfo, sizeof(maskInfo));
    maskInfo.header.biSize = sizeof(BITMAPINFOHEADER);
    maskInfo.header.biWidth = projectedWidth;
    maskInfo.header.biHeight = -projectedHeight;
    maskInfo.header.biPlanes = 1;
    maskInfo.header.biBitCount = 1;
    maskInfo.header.biCompression = BI_RGB;
    maskInfo.header.biClrUsed = 2;
    maskInfo.colors[1].rgbRed = 255;
    maskInfo.colors[1].rgbGreen = 255;
    maskInfo.colors[1].rgbBlue = 255;
    void *projectedMaskBits = 0;
    HBITMAP projectedMask = CreateDIBSection(
        0, reinterpret_cast<const BITMAPINFO *>(&maskInfo), DIB_RGB_COLORS, &projectedMaskBits, 0, 0);

    if (!projectedSource || !projectedSourceBits || !projectedMask || !projectedMaskBits)
    {
      if (projectedMask)
      {
        DeleteObject(projectedMask);
      }
      if (projectedSource)
      {
        DeleteObject(projectedSource);
      }
      return;
    }

    HDC source = CreateCompatibleDC(hdc);
    HDC sourceMask = CreateCompatibleDC(hdc);
    HDC scaledSource = CreateCompatibleDC(hdc);
    HDC scaledMask = CreateCompatibleDC(hdc);
    if (!source || !sourceMask || !scaledSource || !scaledMask)
    {
      if (scaledMask)
      {
        DeleteDC(scaledMask);
      }
      if (scaledSource)
      {
        DeleteDC(scaledSource);
      }
      if (sourceMask)
      {
        DeleteDC(sourceMask);
      }
      if (source)
      {
        DeleteDC(source);
      }
      DeleteObject(projectedMask);
      DeleteObject(projectedSource);
      return;
    }

    HGDIOBJ oldSource = 0;
    HGDIOBJ oldSourceMask = 0;
    HGDIOBJ oldScaledSource = 0;
    HGDIOBJ oldScaledMask = 0;
    const bool selectedSource = SelectRectSurfaceBitmap(source, bitmap, oldSource);
    const bool selectedSourceMask = SelectRectSurfaceBitmap(sourceMask, mask, oldSourceMask);
    const bool selectedScaledSource = SelectRectSurfaceBitmap(scaledSource, projectedSource, oldScaledSource);
    bool selectedScaledMask = SelectRectSurfaceBitmap(scaledMask, projectedMask, oldScaledMask);
    if (selectedSource && selectedSourceMask && selectedScaledSource && selectedScaledMask)
    {
      // COLORONCOLOR is nearest-neighbour here; the one-bit destination keeps
      // the thresholded mask binary while its device extent changes.
      SetStretchBltMode(scaledSource, COLORONCOLOR);
      SetStretchBltMode(scaledMask, COLORONCOLOR);
      const BOOL sourceScaled = StretchBlt(scaledSource,
                                           0,
                                           0,
                                           projectedWidth,
                                           projectedHeight,
                                           source,
                                           0,
                                           0,
                                           width,
                                           height,
                                           SRCCOPY);
      const BOOL maskScaled = StretchBlt(scaledMask,
                                         0,
                                         0,
                                         projectedWidth,
                                         projectedHeight,
                                         sourceMask,
                                         0,
                                         0,
                                         width,
                                         height,
                                         SRCCOPY);
      SelectObject(scaledMask, oldScaledMask);
      selectedScaledMask = false;
      if (sourceScaled && maskScaled)
      {
        MaskBlt(hdc,
                spriteRect.left,
                spriteRect.top,
                projectedWidth,
                projectedHeight,
                scaledSource,
                0,
                0,
                projectedMask,
                0,
                0,
                MAKEROP4(SRCCOPY, kRopKeepDestination));
      }
    }

    if (selectedScaledMask)
    {
      SelectObject(scaledMask, oldScaledMask);
    }
    if (selectedScaledSource)
    {
      SelectObject(scaledSource, oldScaledSource);
    }
    if (selectedSourceMask)
    {
      SelectObject(sourceMask, oldSourceMask);
    }
    if (selectedSource)
    {
      SelectObject(source, oldSource);
    }
    DeleteDC(scaledMask);
    DeleteDC(scaledSource);
    DeleteDC(sourceMask);
    DeleteDC(source);
    DeleteObject(projectedMask);
    DeleteObject(projectedSource);
  }

  void DrawRectSurfaceImage(HDC hdc, const RECT &spriteRect, const loka::app::RectSurfaceSprite &sprite)
  {
    loka::core::resource::Image image;
    if (!sprite.queryImage(image) || !image.isValid())
    {
      return;
    }
    HBITMAP bitmap = static_cast<HBITMAP>(image.nativeHandle());
    if (!bitmap)
    {
      return;
    }
    DIBSECTION dib;
    ZeroMemory(&dib, sizeof(dib));
    if (GetObjectW(bitmap, sizeof(dib), &dib) != sizeof(dib) || !dib.dsBm.bmBits || dib.dsBm.bmBitsPixel != 32)
    {
      return;
    }
    const int width = image.width();
    const int height = image.height();
    if (width <= 0 || height <= 0)
    {
      return;
    }
    if (dib.dsBm.bmWidth < width || (dib.dsBm.bmHeight < 0 ? -dib.dsBm.bmHeight : dib.dsBm.bmHeight) < height)
    {
      return;
    }
    struct MonochromeBitmapInfo
    {
      BITMAPINFOHEADER header;
      RGBQUAD colors[2];
    } maskInfo;
    ZeroMemory(&maskInfo, sizeof(maskInfo));
    maskInfo.header.biSize = sizeof(BITMAPINFOHEADER);
    maskInfo.header.biWidth = width;
    maskInfo.header.biHeight = -height;
    maskInfo.header.biPlanes = 1;
    maskInfo.header.biBitCount = 1;
    maskInfo.header.biCompression = BI_RGB;
    maskInfo.header.biClrUsed = 2;
    maskInfo.colors[1].rgbRed = 255;
    maskInfo.colors[1].rgbGreen = 255;
    maskInfo.colors[1].rgbBlue = 255;
    BYTE *maskBits = 0;
    HBITMAP mask = CreateDIBSection(
        0, reinterpret_cast<const BITMAPINFO *>(&maskInfo), DIB_RGB_COLORS, reinterpret_cast<void **>(&maskBits), 0, 0);
    if (!mask || !maskBits)
    {
      if (mask)
      {
        DeleteObject(mask);
      }
      return;
    }
    const SIZE_T maskRowBytes = static_cast<SIZE_T>(((width + 31) / 32) * 4);
    ZeroMemory(maskBits, maskRowBytes * height);
    const BYTE *sourceBits = static_cast<const BYTE *>(dib.dsBm.bmBits);
    for (int y = 0; y < height; ++y)
    {
      const int sourceY = dib.dsBmih.biHeight < 0 ? y : height - y - 1;
      const BYTE *sourceRow = sourceBits + sourceY * dib.dsBm.bmWidthBytes;
      BYTE *maskRow = maskBits + y * maskRowBytes;
      for (int x = 0; x < width; ++x)
      {
        if (sourceRow[x * 4 + 3] >= 128)
        {
          maskRow[x / 8] |= static_cast<BYTE>(0x80u >> (x % 8));
        }
      }
    }
    const int projectedWidth = spriteRect.right - spriteRect.left;
    const int projectedHeight = spriteRect.bottom - spriteRect.top;
    if (projectedWidth != width || projectedHeight != height)
    {
      // Preserve the existing allocation-free projected==intrinsic path.
      DrawScaledRectSurfaceImage(hdc, spriteRect, bitmap, mask, width, height);
      DeleteObject(mask);
      return;
    }
    HDC source = CreateCompatibleDC(hdc);
    if (!source)
    {
      DeleteObject(mask);
      return;
    }
    HGDIOBJ old = SelectObject(source, bitmap);
    MaskBlt(hdc,
            spriteRect.left,
            spriteRect.top,
            width,
            height,
            source,
            0,
            0,
            mask,
            0,
            0,
            MAKEROP4(SRCCOPY, kRopKeepDestination));
    SelectObject(source, old);
    DeleteDC(source);
    DeleteObject(mask);
  }
}

Win32RectSurfaceContext::Win32RectSurfaceContext(Win32ScenePlatformController *controller,
                                                 HWND parent,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 loka::app::RectSurfaceNode *node)
    : Win32RetirableContext(controller),
      node_(node),
      hwnd_(0),
      modelState_(0)
{
  EnsureClassRegistered();
  hwnd_ = this->createNativeChildWindow(
      0, kRectSurfaceClassName, L"", WS_CHILD | WS_VISIBLE, x, y, width, height, parent, 0, GetModuleHandleW(NULL), this);
  bindModel();
}

Win32RectSurfaceContext::~Win32RectSurfaceContext()
{
  assert(!hwnd_ && "terminal fact delivery must queue the HWND before context reclaim");
}

void Win32RectSurfaceContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void Win32RectSurfaceContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                            loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (next == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
  else
  {
    // DETACHED_RETAINED hides; terminal RETIRED keeps the same policy
    // (hide before the ritual destroys the native pair).
    this->applyDetachedPresentation();
    if (next == loka::app::scene::NODE_FACT_RETIRED)
    {
      this->unbindModel();
      this->retireWindow(this->hwnd_);
      this->node_ = 0;
    }
  }
}

void Win32RectSurfaceContext::applyAttachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
  }
}

void Win32RectSurfaceContext::applyDetachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_HIDE);
  }
}

void Win32RectSurfaceContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  this->positionNativeWindow(this->hwnd_, x, y, width, height);
  HWND parent = 0;
  RECT rect;
  if (this->queryBoundsInParent(parent, rect))
  {
    Win32ScenePlatformController::redrawDirtySubtreeNow(parent, &rect, TRUE);
  }
}

void Win32RectSurfaceContext::EnsureClassRegistered()
{
  static bool registered = false;
  if (registered)
  {
    return;
  }
  WNDCLASSW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = Win32RectSurfaceContext::WndProc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = kRectSurfaceClassName;
  RegisterClassW(&wc);
  registered = true;
}

LRESULT CALLBACK Win32RectSurfaceContext::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  Win32RectSurfaceContext *self =
      static_cast<Win32RectSurfaceContext *>(reinterpret_cast<void *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)));
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
    self = static_cast<Win32RectSurfaceContext *>(create->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  switch (msg)
  {
  case WM_ERASEBKGND:
    Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_RECT_SURFACE, true);
    return 1;
  case WM_PAINT:
  {
    Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_RECT_SURFACE, false);
    PAINTSTRUCT paint;
    HDC hdc = BeginPaint(hwnd, &paint);
    if (self)
    {
      RECT rect;
      GetClientRect(hwnd, &rect);
      self->draw(hdc, rect);
    }
    EndPaint(hwnd, &paint);
    return 0;
  }
  default:
    break;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Win32RectSurfaceContext::bindModel()
{
  if (!node_)
  {
    return;
  }
  modelState_ = node_->props.model_;
  if (modelState_)
  {
    modelState_->bind(&Win32RectSurfaceContext::ModelChangedThunk, this, true);
    applyModel();
  }
}

void Win32RectSurfaceContext::unbindModel()
{
  if (modelState_)
  {
    modelState_->unbind(&Win32RectSurfaceContext::ModelChangedThunk, this);
    modelState_ = 0;
  }
}

void Win32RectSurfaceContext::applyModel()
{
  if (!hwnd_)
  {
    return;
  }
  HWND parent = 0;
  RECT rect;
  if (this->queryBoundsInParent(parent, rect))
  {
    Win32ScenePlatformController::requestDirtySubtree(parent, &rect, TRUE);
    return;
  }
  Win32ScenePlatformController::requestDirtyRect(hwnd_, NULL, TRUE);
}

bool Win32RectSurfaceContext::queryBoundsInParent(HWND &parent, RECT &rect) const
{
  parent = this->hwnd_ ? GetParent(this->hwnd_) : 0;
  if (!parent || !GetWindowRect(this->hwnd_, &rect))
  {
    return false;
  }
  MapWindowPoints(NULL, parent, reinterpret_cast<POINT *>(&rect), 2);
  return true;
}

void Win32RectSurfaceContext::ModelChangedThunk(void *userData)
{
  Win32RectSurfaceContext *self = static_cast<Win32RectSurfaceContext *>(userData);
  if (self)
  {
    self->applyModel();
  }
}

void Win32RectSurfaceContext::draw(HDC hdc, const RECT &rect)
{
  if (node_ && node_->props.clearBackground_)
  {
    HBRUSH backgroundBrush = CreateSolidBrush(kRectSurfaceClearColor);
    if (backgroundBrush)
    {
      FillRect(hdc, &rect, backgroundBrush);
      DeleteObject(backgroundBrush);
    }
  }
  if (!node_ || !modelState_)
  {
    return;
  }
  const loka::app::RectSurfaceModel model = modelState_->get();
  const loka::app::RectSurfacePaintList paintList(model);
  HBRUSH blackBrush = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  for (short i = 0; i < paintList.count(); ++i)
  {
    const loka::app::RectSurfaceSprite &sprite = *paintList.querySprite(i);
    switch (sprite.kind())
    {
    case loka::app::RectSurfaceSprite::KIND_RECT:
      break;
    case loka::app::RectSurfaceSprite::KIND_IMAGE:
    {
      RECT spriteRect;
      const loka::core::Frame logicalRect(sprite.x, sprite.y, sprite.width, sprite.height);
      this->controller()->displayScale().projectFrame(logicalRect, spriteRect);
      DrawRectSurfaceImage(hdc, spriteRect, sprite);
      continue;
    }
    }
    RECT spriteRect;
    const loka::core::Frame logicalRect(sprite.x, sprite.y, sprite.width, sprite.height);
    this->controller()->displayScale().projectFrame(logicalRect, spriteRect);
    FillRect(hdc, &spriteRect, blackBrush);
  }
}
