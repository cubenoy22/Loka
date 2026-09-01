#include "Win32TextContext.hpp"
#include <cassert>
#include "../Win32ScenePlatformController.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "app/nodes/Text.hpp"
#include "core/resource/Image.hpp"
#include "core/State.hpp"
#include "platform/Win32String.hpp"

namespace
{
  class Win32TextNodeHandler
      : public loka::app::scene::RetainedNodeHandler<Win32TextNodeHandler,
                                                     loka::app::TextNode,
                                                     Win32TextContext>
  {
  public:
    static loka::app::TextNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asTextNode() : 0;
    }

    static Win32TextContext *create(loka::app::TextNode *text,
                                    loka::app::scene::IPlatformController *controller,
                                    const loka::app::scene::LayoutState &state)
    {
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      return new Win32TextContext(
          win32, win32->projectionParentHwnd(), state.x, state.y, state.width, state.height, text);
    }

    static void refresh(Win32TextContext *ctx, const loka::app::scene::LayoutState &state)
    {
      ctx->relayout(state.x, state.y, state.width, state.height);
    }
  };

  Win32TextNodeHandler gWin32TextNodeHandler;

  int MeasureTextHeightForWidth(HWND hwnd, const loka::app::TextNode *text, int width, int defaultHeight)
  {
    if (!hwnd || !text || !text->props.text_)
    {
      return defaultHeight;
    }
    if (!text->props.hasAttr_ || !text->props.attr_.hasWrapValue_
        || text->props.attr_.wrapValue_ == loka::app::TEXT_WRAP_NONE)
    {
      return defaultHeight;
    }
    if (width <= 0)
    {
      return defaultHeight;
    }

    std::wstring wide;
    if (!loka::win32::MaterializeWideString(text->props.text_->get(), wide))
    {
      return defaultHeight;
    }
    if (wide.empty())
    {
      return defaultHeight;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc)
    {
      return defaultHeight;
    }
    RECT rc;
    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = 0;
    UINT flags = DT_LEFT | DT_NOPREFIX | DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL;
    DrawTextW(hdc, wide.c_str(), -1, &rc, flags);
    ReleaseDC(hwnd, hdc);

    const int measured = rc.bottom - rc.top;
    const int measuredWithPadding = measured + 8;
    if (measuredWithPadding > defaultHeight)
    {
      return measuredWithPadding;
    }
    return defaultHeight;
  }

  void ReleaseCapturedBitmap(void *handle, void *)
  {
    if (handle)
    {
      DeleteObject(static_cast<HBITMAP>(handle));
    }
  }

  bool CaptureWindowBitmap(HWND hwnd, loka::core::resource::Image &out)
  {
    out = loka::core::resource::Image::Empty();
    if (!hwnd)
    {
      return false;
    }

    RECT rc;
    if (!GetClientRect(hwnd, &rc))
    {
      return false;
    }
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0)
    {
      return false;
    }

    HDC windowDC = GetWindowDC(hwnd);
    if (!windowDC)
    {
      return false;
    }

    HDC memDC = CreateCompatibleDC(windowDC);
    if (!memDC)
    {
      ReleaseDC(hwnd, windowDC);
      return false;
    }

    HBITMAP bitmap = CreateCompatibleBitmap(windowDC, width, height);
    if (!bitmap)
    {
      DeleteDC(memDC);
      ReleaseDC(hwnd, windowDC);
      return false;
    }

    HGDIOBJ oldBitmap = SelectObject(memDC, bitmap);
    const BOOL copied = BitBlt(memDC, 0, 0, width, height, windowDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd, windowDC);
    if (!copied)
    {
      DeleteObject(bitmap);
      return false;
    }

    out = loka::core::resource::Image::FromNative(bitmap, width, height, &ReleaseCapturedBitmap, 0);
    return out.isValid();
  }
} // namespace

Win32TextContext::Win32TextContext(Win32ScenePlatformController *controller,
                                   HWND parent,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   loka::app::TextNode *node)
    : Win32RetirableContext(controller),
      node_(node),
      hwnd_(NULL),
      textState_(0),
      didInitialApply_(false)
{
  DWORD style = WS_VISIBLE | WS_CHILD | SS_LEFT;
  if (node_ && node_->props.hasAttr_)
  {
    const loka::app::TextAttr &attr = node_->props.attr_;
    const bool wrapEnabled =
        attr.hasWrapValue_
        && (attr.wrapValue_ == loka::app::TEXT_WRAP_WORD || attr.wrapValue_ == loka::app::TEXT_WRAP_CHAR);
    const bool truncEllipsis = attr.hasTruncationValue_ && attr.truncationValue_ == loka::app::TEXT_TRUNCATION_ELLIPSIS;
    if (!wrapEnabled)
    {
      style |= SS_LEFTNOWORDWRAP;
    }
    else
    {
      style |= SS_EDITCONTROL;
    }
    if (truncEllipsis)
    {
      style |= SS_ENDELLIPSIS;
    }
  }
  // Unicode window: keeps WM_SETTEXT/paint in UTF-16 so the displayed text
  // matches what MeasureTextHeightForWidth measures with DrawTextW.
  hwnd_ = CreateWindowExW(0, L"STATIC", L"", style, x, y, width, height, parent, NULL, GetModuleHandle(NULL), NULL);
  if (hwnd_)
  {
    HDC hdc = GetDC(hwnd_);
    if (hdc)
    {
      SetBkMode(hdc, TRANSPARENT);
      ReleaseDC(hwnd_, hdc);
    }
  }
  bindText();
}

Win32TextContext::~Win32TextContext()
{
  assert(!hwnd_ && "terminal fact delivery must queue the HWND before context reclaim");
}

void Win32TextContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void Win32TextContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
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
      this->unbindText();
      this->retireWindow(this->hwnd_);
      this->node_ = 0;
    }
  }
}

void Win32TextContext::applyAttachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
  }
}

void Win32TextContext::applyDetachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_HIDE);
  }
}

bool Win32TextContext::captureBitmap(loka::core::resource::Image &out) const
{
  return CaptureWindowBitmap(this->hwnd_, out);
}

short Win32TextContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  const int textHeight = MeasureTextHeightForWidth(
      this->hwnd_, this->node_, state.width, loka::app::layout::FallbackControlMetrics::kTextHeight);
  this->relayout(state.x, state.y, state.width, textHeight);
  state.height = static_cast<short>(textHeight);
  return static_cast<short>(state.y + textHeight + loka::app::layout::FallbackControlMetrics::kVerticalSpacing);
}

void Win32TextContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  this->positionNativeWindow(this->hwnd_, x, y, width, height);
}

void Win32TextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->bind(&Win32TextContext::TextChangedThunk, this, true);
  }
}

void Win32TextContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&Win32TextContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32TextContext::applyText()
{
  if (!hwnd_ || !textState_)
  {
    return;
  }
  std::wstring wide;
  if (loka::win32::MaterializeWideString(textState_->get(), wide))
  {
    SetWindowTextW(hwnd_, wide.c_str());
  }
  else
  {
    SetWindowTextW(hwnd_, L"");
  }
  HWND parent = GetParent(hwnd_);
  if (parent)
  {
    RECT rc;
    if (GetWindowRect(hwnd_, &rc))
    {
      MapWindowPoints(NULL, parent, reinterpret_cast<POINT *>(&rc), 2);
      Win32ScenePlatformController::redrawDirtySubtreeNow(parent, &rc, TRUE);
    }
  }
  requestRelayoutIfNeeded();
  if (!didInitialApply_)
  {
    didInitialApply_ = true;
  }
}

void Win32TextContext::requestRelayoutIfNeeded()
{
  if (!didInitialApply_ || !node_ || !node_->props.hasAttr_ || !node_->props.attr_.hasWrapValue_)
  {
    return;
  }
  if (node_->props.attr_.wrapValue_ == loka::app::TEXT_WRAP_NONE)
  {
    return;
  }
  HWND parent = GetParent(hwnd_);
  if (!parent)
  {
    return;
  }
  RECT rc;
  if (!GetClientRect(parent, &rc))
  {
    return;
  }
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;
  PostMessage(parent, WM_SIZE, static_cast<WPARAM>(SIZE_RESTORED), static_cast<LPARAM>(MAKELPARAM(width, height)));
}

void Win32TextContext::TextChangedThunk(void *userData)
{
  Win32TextContext *self = static_cast<Win32TextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}

void RegisterWin32TextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32TextNodeHandler);
}
