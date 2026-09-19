#include "Win32TextContext.hpp"
#include <cassert>
#include "../Win32ScenePlatformController.hpp"
#include "../Win32BitmapCapture.hpp"
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

  HFONT ResolveTextFont(const loka::app::TextNode *text,
                        const Win32ScenePlatformController *controller)
  {
    if (!text || !controller || !text->props.hasDeclaredStyle())
      return 0;
    const loka::app::TextStyle style = text->props.resolvedTextStyle();
    return style.hasFontSize_ || style.hasWeight_ || style.hasItalic_
               ? controller->textFont(style) : 0;
  }

  /** A null selected font preserves the historical unstyled height. */
  int MinimumTextHeight(HWND hwnd, HFONT font, const Win32ScenePlatformController *controller)
  {
    const int fallback = loka::app::layout::FallbackControlMetrics::kTextHeight;
    if (!hwnd || !font || !controller)
      return fallback;
    HDC hdc = GetDC(hwnd);
    if (!hdc)
      return fallback;
    HGDIOBJ previous = SelectObject(hdc, font);
    TEXTMETRICW metrics;
    const bool measured = GetTextMetricsW(hdc, &metrics) != FALSE;
    if (previous)
      SelectObject(hdc, previous);
    ReleaseDC(hwnd, hdc);
    const int height =
        measured ? controller->displayScale().measurementToLu(metrics.tmHeight + metrics.tmExternalLeading) : fallback;
    return height > fallback ? height : fallback;
  }

  int MeasureTextHeightForWidth(HWND hwnd,
                                const Win32ScenePlatformController *controller,
                                const loka::app::TextNode *text,
                                int width,
                                int defaultHeight,
                                HFONT selectedFont)
  {
    if (!hwnd || !controller || !text || !text->props.text_)
    {
      return defaultHeight;
    }
    if (!text->props.blockStyle_.hasWrap_
        || text->props.blockStyle_.wrap_ == loka::app::TEXT_WRAP_NONE)
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
    rc.right = controller->displayScale().nativeLength(0, width).px;
    rc.bottom = 0;
    HGDIOBJ previousFont = 0;
    if (selectedFont)
    {
      previousFont = SelectObject(hdc, selectedFont);
    }
    UINT flags = DT_LEFT | DT_NOPREFIX | DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL;
    DrawTextW(hdc, wide.c_str(), -1, &rc, flags);
    if (previousFont)
    {
      SelectObject(hdc, previousFont);
    }
    ReleaseDC(hwnd, hdc);

    const int measured = controller->displayScale().measurementToLu(rc.bottom - rc.top);
    const int measuredWithPadding = measured + 8;
    if (measuredWithPadding > defaultHeight)
    {
      return measuredWithPadding;
    }
    return defaultHeight;
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
      didInitialApply_(false),
      textDelivery_(loka::app::scene::PaintAnswer::refused(loka::app::scene::PAINT_REFUSED_HISTORY_UNKNOWN))
{
  DWORD style = WS_VISIBLE | WS_CHILD | SS_LEFT;
  if (node_ && node_->props.hasDeclaredStyle())
  {
    const loka::app::BlockStyle &attr = node_->props.blockStyle_;
    const bool wrapEnabled =
        attr.hasWrap_
        && (attr.wrap_ == loka::app::TEXT_WRAP_WORD || attr.wrap_ == loka::app::TEXT_WRAP_CHAR);
    const bool truncEllipsis = attr.hasTruncation_ && attr.truncation_ == loka::app::TEXT_TRUNCATION_ELLIPSIS;
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
  hwnd_ = this->createNativeChildWindow(
      0,
      L"STATIC",
      L"",
      style,
      this->controller()->displayScale().projectFrame(loka::core::Frame(x, y, width, height)),
      parent,
      NULL,
      GetModuleHandleW(NULL),
      NULL);
  if (hwnd_)
  {
    HDC hdc = GetDC(hwnd_);
    if (hdc)
    {
      SetBkMode(hdc, TRANSPARENT);
      ReleaseDC(hwnd_, hdc);
    }
  }
  this->applyStyle();
  bindText();
}

Win32TextContext::~Win32TextContext()
{
  assert(!hwnd_ && "terminal fact delivery must queue the HWND before context reclaim");
}

/** A changed STATIC text has already requested its parent rectangle with
    erase=false, children=true. An unchanged native string owes no new damage. */
loka::app::scene::PaintAnswer Win32TextContext::queryPaintDamage(const loka::app::scene::PaintQuery &query) const
{
  using namespace loka::app::scene;
  if (!this->hwnd_)
    return PaintAnswer::refused(PAINT_REFUSED_NO_CONTEXT);
  if (query.placement != PLACEMENT_ELIGIBLE)
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (!this->node_ || (!this->node_->props.ownsText && this->node_->props.text_ != this->textState_))
    return PaintAnswer::refused(PAINT_REFUSED_PROPS_UNRECONCILED);
  PaintAnswer answer = this->textDelivery_;
  if (answer.kind == PAINT_ANSWER_EXACT)
    answer.damage.scope = query.scope;
  return answer;
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
    this->textDelivery_ = loka::app::scene::PaintAnswer::refused(loka::app::scene::PAINT_REFUSED_HISTORY_UNKNOWN);
    this->applyDetachedPresentation();
    if (next == loka::app::scene::NODE_FACT_RETIRED)
    {
      this->unbindText();
      this->retireWindow(this->hwnd_);
      this->node_ = 0;
    }
  }
}

bool Win32TextContext::applyStyle()
{
  if (!this->hwnd_ || !this->node_ || !this->controller())
    return false;
  HFONT font = ResolveTextFont(this->node_, this->controller());
  if (!font)
    font = this->controller()->displayFont();
  // WM_GETFONT is the native truth; no cached handle can outlive a DPI table.
  if (font && reinterpret_cast<HFONT>(SendMessageW(this->hwnd_, WM_GETFONT, 0, 0)) != font)
  {
    SendMessageW(this->hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    InvalidateRect(this->hwnd_, NULL, TRUE);
    return true;
  }
  return false;
}

void Win32TextContext::onPropsApplied()
{
  if (this->applyStyle())
    this->controller()->requestRelayout();
  if (!this->node_)
  {
    return;
  }
  if (this->node_->props.ownsText)
  {
    // Props-owned text is not a live source (TextNode::declareDirtySources
    // classifies ownsText as non-live): a literal-to-literal apply rewrites the
    // same owned State without notifying. Treat it as an applied snapshot.
    this->unbindText();
    this->applyText();
    return;
  }
  if (this->node_->props.text_ != this->textState_)
  {
    this->unbindText();
    this->bindText();
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
  return loka::win32::CaptureWindowClientBitmap(this->hwnd_, out);
}

short Win32TextContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  this->applyStyle();
  const HFONT font = ResolveTextFont(this->node_, this->controller());
  const int textHeight = MeasureTextHeightForWidth(
      this->hwnd_,
      this->controller(),
      this->node_,
      state.width,
      MinimumTextHeight(this->hwnd_, font, this->controller()),
      font ? font : this->controller()->displayFont());
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
  this->positionNativeWindow(this->hwnd_,
                             this->controller()->displayScale().projectFrame(loka::core::Frame(x, y, width, height)));
}

void Win32TextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  // Subscribe only to a borrowed live State; props-owned text is applied as a
  // snapshot (see onPropsApplied) and never subscribed to.
  textState_ = node_->props.ownsText ? 0 : static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->bind(&Win32TextContext::TextChangedThunk, this, false);
  }
  this->applyText();
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
  using namespace loka::app::scene;
  this->textDelivery_ = PaintAnswer::refused(PAINT_REFUSED_PROPS_UNRECONCILED);
  // Always read the node's current props: for a borrowed State this is the
  // subscribed one, for owned text it is the latest applied literal.
  if (!hwnd_ || !node_ || !node_->props.text_)
  {
    return;
  }
  std::wstring wide;
  if (!loka::win32::MaterializeWideString(node_->props.text_->get(), wide))
    wide.clear();
  // Compare lengths first; only matching short strings need a native read.
  // Longer strings conservatively count as changed, so comparison never
  // allocates a previous-text buffer on the apply path.
  wchar_t previous[256];
  const int length = GetWindowTextLengthW(this->hwnd_);
  const bool unchanged = this->didInitialApply_ && static_cast<size_t>(length) == wide.size()
                         && wide.size() < sizeof(previous) / sizeof(previous[0])
                         && GetWindowTextW(this->hwnd_, previous, sizeof(previous) / sizeof(previous[0])) == length
                         && wide == previous;
  const bool applied = SetWindowTextW(this->hwnd_, wide.c_str()) != FALSE;
  HWND parent = GetParent(hwnd_);
  if (parent)
  {
    RECT rc;
    if (GetWindowRect(hwnd_, &rc))
    {
      MapWindowPoints(NULL, parent, reinterpret_cast<POINT *>(&rc), 2);
      Win32ScenePlatformController::requestDirtySubtree(parent, &rc, FALSE);
      if (applied)
      {
        const PaintDamage empty = {paintScope(), 0, 0, 0, 0, PAINT_COVERAGE_PAINT_ONLY};
        this->textDelivery_ = unchanged ? PaintAnswer::exact(empty) : PaintAnswer::nativeScheduled();
      }
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
  if (!didInitialApply_ || !node_ || !node_->props.blockStyle_.hasWrap_)
  {
    return;
  }
  if (node_->props.blockStyle_.wrap_ == loka::app::TEXT_WRAP_NONE)
  {
    return;
  }
  if (this->controller())
    this->controller()->requestRelayout();
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
