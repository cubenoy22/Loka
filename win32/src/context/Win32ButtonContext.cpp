#include "Win32ButtonContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "app/nodes/controls/Button.hpp"
#include "core/resource/Image.hpp"
#include "core/State.hpp"
#include "platform/Win32String.hpp"

namespace
{
  const int kButtonHeight = 32;
  const int kVerticalSpacing = 12;

  class Win32ButtonNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ButtonNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::ButtonNode *button = node ? node->asButtonNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!button || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureButtonContext(button, state.x, state.y, state.width, state.height);
    }
  };

  Win32ButtonNodeHandler gWin32ButtonNodeHandler;

  void ReleaseCapturedButtonBitmap(void *handle, void *)
  {
    if (handle)
    {
      DeleteObject(static_cast<HBITMAP>(handle));
    }
  }

  bool CaptureButtonBitmap(HWND hwnd, loka::core::resource::Image &out)
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

    out = loka::core::resource::Image::FromNative(bitmap, width, height, &ReleaseCapturedButtonBitmap, 0);
    return out.isValid();
  }
} // namespace

Win32ButtonContext::Win32ButtonContext(HWND parent, int x, int y, int width, int height, loka::app::ButtonNode *node)
    : node_(node),
      hwnd_(NULL),
      textState_(0),
      enabledState_(0)
{
  DWORD style = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON;
  // Unicode window: keeps the label UTF-16 end to end (an ANSI window would
  // thunk SetWindowTextW through the system ACP and lose out-of-ACP chars).
  hwnd_ = CreateWindowExW(0,
                          L"BUTTON",
                          L"",
                          style,
                          x,
                          y,
                          width,
                          height,
                          parent,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(1000)),
                          GetModuleHandle(NULL),
                          NULL);
  if (hwnd_)
  {
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  }
  bindText();
  bindEnabled();
}

Win32ButtonContext::~Win32ButtonContext()
{
  unbindText();
  unbindEnabled();
  if (hwnd_)
  {
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

void Win32ButtonContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void Win32ButtonContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
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
  }
}

void Win32ButtonContext::applyAttachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
  }
}

void Win32ButtonContext::applyDetachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_HIDE);
  }
}

bool Win32ButtonContext::captureBitmap(loka::core::resource::Image &out) const
{
  return CaptureButtonBitmap(this->hwnd_, out);
}

short Win32ButtonContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  this->relayout(state.x, state.y, state.width, kButtonHeight);
  state.height = static_cast<short>(kButtonHeight);
  return static_cast<short>(state.y + kButtonHeight + kVerticalSpacing);
}

bool Win32ButtonContext::handleCommand(WPARAM, LPARAM)
{
  if (node_ && node_->props.onClick_)
  {
    node_->props.onClick_->emit();
    return true;
  }
  return false;
}

void Win32ButtonContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  MoveWindow(hwnd_, x, y, width, height, TRUE);
}

void Win32ButtonContext::bindText()
{
  if (!node_)
    return;
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->bind(&Win32ButtonContext::TextChangedThunk, this, true);
  }
}

void Win32ButtonContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&Win32ButtonContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32ButtonContext::bindEnabled()
{
  if (!node_)
    return;
  enabledState_ = static_cast<loka::core::State<bool> *>(node_->props.enabled_);
  if (enabledState_)
  {
    enabledState_->bind(&Win32ButtonContext::EnabledChangedThunk, this, true);
  }
}

void Win32ButtonContext::unbindEnabled()
{
  if (enabledState_)
  {
    enabledState_->unbind(&Win32ButtonContext::EnabledChangedThunk, this);
    enabledState_ = 0;
  }
}

void Win32ButtonContext::applyText()
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
  Win32ScenePlatformController::requestDirtyRect(hwnd_, NULL, TRUE);
}

void Win32ButtonContext::applyEnabled()
{
  if (!hwnd_ || !enabledState_)
  {
    return;
  }
  EnableWindow(hwnd_, enabledState_->get() ? TRUE : FALSE);
  Win32ScenePlatformController::requestDirtyRect(hwnd_, NULL, TRUE);
}

void Win32ButtonContext::TextChangedThunk(void *userData)
{
  Win32ButtonContext *self = static_cast<Win32ButtonContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}

void Win32ButtonContext::EnabledChangedThunk(void *userData)
{
  Win32ButtonContext *self = static_cast<Win32ButtonContext *>(userData);
  if (self)
  {
    self->applyEnabled();
  }
}

void RegisterWin32ButtonNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32ButtonNodeHandler);
}
