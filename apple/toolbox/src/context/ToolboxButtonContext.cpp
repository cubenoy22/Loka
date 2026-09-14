#include "ToolboxPropsRefresh.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxLayoutMetrics.hpp"
#include "context/ToolboxLayoutUtil.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "platform/StringUTF8.hpp"
#include <cstring>
#include <string>

namespace
{
  class ToolboxButtonNodeHandler
      : public loka::app::scene::RetainedNodeHandler<ToolboxButtonNodeHandler,
                                                     loka::app::ButtonNode,
                                                     ToolboxButtonContext>
  {
  public:
    static loka::app::ButtonNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asButtonNode() : 0;
    }

    static ToolboxButtonContext *create(loka::app::ButtonNode *node,
                                        loka::app::scene::IPlatformController *controller,
                                        const loka::app::scene::LayoutState &state)
    {
      (void)state;
      return new ToolboxButtonContext(node, static_cast<ToolboxScenePlatformController *>(controller));
    }
  };

  ToolboxButtonNodeHandler gToolboxButtonNodeHandler;

  void DrawStringAt(short x, short y, const loka::core::String &value)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return;
    }
    std::size_t length = utf8.size();
    if (length > 255)
    {
      length = 255;
    }
    Str255 text;
    text[0] = static_cast<unsigned char>(length);
    if (length > 0)
    {
      std::memcpy(text + 1, utf8.data(), length);
    }
    MoveTo(x, y);
    DrawString(text);
  }
} // namespace

ToolboxButtonContext::ToolboxButtonContext(loka::app::ButtonNode *node, ToolboxScenePlatformController *controller)
    : ToolboxProjectedNodeContext(controller),
      node_(node),
      rect_(),
      paintRect_(),
      label_(loka::core::String::Literal("Button")),
      emitter_(0),
      enabled_(0),
      resourceId_(0)
{
}

ToolboxButtonContext::~ToolboxButtonContext() {}

loka::app::scene::PaintAnswer ToolboxButtonContext::queryPaintDamage(const loka::app::scene::PaintQuery &query) const
{
  using namespace loka::app::scene;
  if (query.placement != PLACEMENT_ELIGIBLE || query.scope != ToolboxPaintScope())
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (!this->node_)
    return PaintAnswer::refused(PAINT_REFUSED_HISTORY_UNKNOWN);
  const ToolboxButtonPaintValue current(
      this->node_->props.text_ ? this->node_->props.text_->get() : loka::core::String::Literal("Button"),
      !this->node_->props.enabled_ || this->node_->props.enabled_->get());
  if (ToolboxPaintIsClippedOut(this->rect_, this->paintRect_, this->deliveredFact()))
  {
    // No presented value is required offscreen, but the laid-out title width
    // still constrains placement. A changed width can move visible siblings.
    if (ToolboxMeasureTextWidth(current.label()) != this->rect_.right - this->rect_.left)
      return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
    return ToolboxExactPaint(this->paintRect_, false);
  }
  if (!this->presented_.isKnown())
    return PaintAnswer::refused(PAINT_REFUSED_HISTORY_UNKNOWN);
  // The control's width is derived from its title (layout), so a title whose
  // measured width differs from the presented one moves this button and its
  // Row siblings: that is layout work, not paint, and exact delivery refuses.
  if (ToolboxMeasureTextWidth(current.label()) != ToolboxMeasureTextWidth(this->presented_.value().label()))
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  return ToolboxExactPaint(this->paintRect_, !(current == this->presented_.value()));
}

void ToolboxButtonContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                        loka::app::scene::NodeLifecycleFact next)
{
  if (next != loka::app::scene::NODE_FACT_ATTACHED)
    this->presented_.invalidate();
  ToolboxProjectedNodeContext::onFactChanged(previous, next);
}

void ToolboxButtonContext::retireNativeProjection()
{
  if (this->controller())
  {
    // The terminal fact delivery just refreshed the hint snapshot, so the
    // retire flush decides on the freshest value — not the last render's.
    this->controller()->destroyButtonControl(resourceId_, this->lifetimeHint());
  }
  this->node_ = 0;
}

void ToolboxButtonContext::updateData(const loka::core::String &label,
                                      loka::core::EmitterState *emitter,
                                      loka::core::State<bool> *enabled,
                                      short resourceId,
                                      int controlTag)
{
  label_ = label;
  emitter_ = emitter;
  enabled_ = enabled;
  if (resourceId > 0)
  {
    resourceId_ = resourceId;
  }
  else if (controlTag > 0 && controlTag <= 32767)
  {
    resourceId_ = static_cast<short>(controlTag);
  }
}

void ToolboxButtonContext::updateRect(const Rect &rect)
{
  Rect paintRect = rect;
  if (this->controller() && !this->controller()->intersectWithProjectionClip(rect, paintRect))
    SetRect(&paintRect, 0, 0, 0, 0);
  if (!EqualRect(&this->rect_, &rect) || !EqualRect(&this->paintRect_, &paintRect))
    this->presented_.invalidate();
  this->rect_ = rect;
  this->paintRect_ = paintRect;
}

void ToolboxButtonContext::draw(ToolboxScenePlatformController *controller)
{
  ToolboxPaintClip clip(this->paintRect_);
  const bool paints = !clip.isActive() || clip.touches(this->paintRect_);
  if (paints)
    this->presented_.invalidate();
  // ensureButtonControl also marks the native control used by this render.
  // Keep that registration even when its pixels are outside the caller's clip.
  if (controller && resourceId_ <= 0)
  {
    resourceId_ = controller->allocateControlId();
  }
  if (controller && resourceId_ > 0)
  {
    if (controller->ensureButtonControl(resourceId_, rect_, label_, emitter_, enabled_, lifetimeHint(), this))
    {
      if (!EmptyRect(&this->paintRect_) && clip.covers(this->paintRect_))
        this->presented_.commit(ToolboxButtonPaintValue(this->label_, !this->enabled_ || this->enabled_->get()),
                                ToolboxPaintScope());
      return;
    }
  }
  if (paints)
  {
    FrameRect(&rect_);
    DrawStringAt(static_cast<short>(rect_.left + 4), static_cast<short>(rect_.bottom - ToolboxLayoutMetrics::kControlDescent), label_);
  }
  if (controller)
  {
    controller->recordButtonHit(rect_, emitter_, enabled_, boundary_, this);
  }
}

bool ReconcileToolboxButtonControl(ControlRef control, const loka::core::String &label,
                                   loka::core::State<bool> *enabled, std::string &installedLabel)
{
  if (!control)
    return false;
  std::string labelUtf8;
  if (!loka::platform::CollectUtf8(label, labelUtf8))
    return false;
  if (installedLabel != labelUtf8)
  {
    const std::size_t length = labelUtf8.size() > 255 ? 255 : labelUtf8.size();
    Str255 title;
    title[0] = static_cast<unsigned char>(length);
    if (length > 0)
      std::memcpy(title + 1, labelUtf8.data(), length);
    SetControlTitle(control, title);
    installedLabel = labelUtf8;
  }
  HiliteControl(control, enabled && !enabled->get() ? 255 : 0);
  return true;
}

void ToolboxButtonContext::repaint(ControlRef control, std::string &installedLabel)
{
  ToolboxPaintClip clip(this->paintRect_);
  if (clip.isActive() && !clip.touches(this->paintRect_))
    return;
  this->presented_.invalidate();
  if (!this->node_ || !control)
    return;
  const loka::core::String label = this->node_->props.text_
      ? this->node_->props.text_->get() : loka::core::String::Literal("Button");
  const bool submitted = ReconcileToolboxButtonControl(control, label, this->node_->props.enabled_, installedLabel);
  Draw1Control(control);
  if (submitted && !EmptyRect(&this->paintRect_) && clip.covers(this->paintRect_))
    this->presented_.commit(ToolboxButtonPaintValue(label,
        !this->node_->props.enabled_ || this->node_->props.enabled_->get()), ToolboxPaintScope());
}

void ToolboxButtonContext::forgetPresentedControl()
{
  this->presented_.invalidate();
}

short ToolboxButtonContext::layout(loka::app::scene::IPlatformController *controller,
                                   loka::app::scene::LayoutState &state)
{
  (void)controller;
  if (!node_)
  {
    return 0;
  }
  this->captureProps();
  short width = ToolboxMeasureTextWidth(this->label_);
  Rect rect;
  rect.left = state.x;
  rect.top = state.y;
  rect.right = static_cast<short>(state.x + width);
  rect.bottom = static_cast<short>(state.y + state.lineHeight - ToolboxLayoutMetrics::kControlAscentInset
                                   + ToolboxLayoutMetrics::kControlDescent);
  updateRect(rect);
  // Advance by the painted box, as the other rails do: y is the top edge.
  state.y = static_cast<short>(rect.bottom + state.spacing);
  return width;
}

void ToolboxButtonContext::render(loka::app::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}

bool ToolboxButtonContext::handleMouseDown(const Point &point, ToolboxScenePlatformController *controller)
{
  if (!emitter_)
  {
    return false;
  }
  if (enabled_ && !enabled_->get())
  {
    return false;
  }
  if (!PtInRect(point, &rect_))
  {
    return false;
  }
  if (controller)
  {
    controller->emitHitEmitter(emitter_);
  }
  return true;
}

bool RegisterToolboxButtonNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  return registry.registerHandler(&gToolboxButtonNodeHandler);
}

bool ToolboxButtonContext::captureProps()
{
  if (!this->node_)
    return false;
  const loka::core::String label =
      this->node_->props.text_ ? this->node_->props.text_->get() : loka::core::String::Literal("Button");
  const bool changed = ToolboxButtonProjectionChanged(
      this->label_, label, this->enabled_, this->node_->props.enabled_,
      this->emitter_, this->node_->props.onClick_);
  this->updateData(label,
                   this->node_->props.onClick_,
                   this->node_->props.enabled_,
                   this->resourceId_,
                   this->node_->props.controlTag_);
  return changed;
}

void ToolboxButtonContext::onPropsApplied()
{
  this->presented_.invalidate();
  const bool changed = this->captureProps();
  if (changed && this->controller() && this->node_)
  {
    this->controller()->refreshContextProps(this->node_, this->resourceId_);
  }
}
