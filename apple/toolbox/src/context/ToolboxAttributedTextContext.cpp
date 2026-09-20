#include "context/ToolboxAttributedTextContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include <climits>

namespace
{
  class ToolboxAttributedTextNodeHandler
      : public loka::app::scene::RetainedNodeHandler<ToolboxAttributedTextNodeHandler,
                                                     loka::app::AttributedTextNode,
                                                     ToolboxAttributedTextContext>
  {
  public:
    static loka::app::AttributedTextNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asAttributedTextNode() : 0;
    }
    static ToolboxAttributedTextContext *create(loka::app::AttributedTextNode *node,
                                                loka::app::scene::IPlatformController *controller,
                                                const loka::app::scene::LayoutState &)
    {
      return new ToolboxAttributedTextContext(node, static_cast<ToolboxScenePlatformController *>(controller));
    }
  };
  ToolboxAttributedTextNodeHandler handler;
  short Coordinate(int value)
  {
    return static_cast<short>(value > SHRT_MAX ? SHRT_MAX : value < SHRT_MIN ? SHRT_MIN : value);
  }
} // namespace

ToolboxAttributedTextContext::ToolboxAttributedTextContext(loka::app::AttributedTextNode *node,
                                                           ToolboxScenePlatformController *controller)
    : ToolboxProjectedNodeContext(controller),
      node_(node),
      rect_(),
      paintRect_()
{
}

ToolboxAttributedTextContext::~ToolboxAttributedTextContext()
{
  assert(!this->table_.valid() && "retirement must drop derived text before reclaim");
}

void ToolboxAttributedTextContext::readLifecycleFactOnAttach()
{
  if (this->controller())
    this->controller()->registerCompositionReplay(this->replay_);
}

void ToolboxAttributedTextContext::retireNativeProjection()
{
  this->replay_.clear();
  this->table_.clear();
}

void ToolboxAttributedTextContext::onPropsApplied()
{
  this->table_.clear();
  this->presented_.invalidate();
}

void ToolboxAttributedTextContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                                 loka::app::scene::NodeLifecycleFact next)
{
  if (next != loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->presented_.invalidate();
    SetRect(&this->paintRect_, 0, 0, 0, 0);
  }
  ToolboxProjectedNodeContext::onFactChanged(previous, next);
}

short ToolboxAttributedTextContext::layout(loka::app::scene::IPlatformController *controller,
                                           loka::app::scene::LayoutState &state)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  if (!toolbox || !this->node_ || !this->node_->props.text_)
  {
    this->onPropsApplied();
    return 0;
  }
  assert(toolbox->textShaping() == loka::app::PER_RUN);
  const loka::app::AttributedString &value = this->node_->props.text_->get();
  if (!this->table_.build(value, this->node_->props.blockStyle_, state.width, *toolbox))
  {
    this->presented_.invalidate();
    return 0;
  }
  this->presented_.invalidate();
  const short width = state.width > 0 ? state.width : this->table_.lines().width();
  this->rect_.left = state.x;
  this->rect_.top = state.y;
  this->rect_.right = Coordinate(state.x + width);
  this->rect_.bottom = Coordinate(state.y + this->table_.height());
  this->paintRect_ = this->rect_;
  if (!toolbox->intersectWithProjectionClip(this->rect_, this->paintRect_))
    SetRect(&this->paintRect_, 0, 0, 0, 0);
  state.y = Coordinate(this->rect_.bottom + state.spacing);
  return width;
}

loka::app::scene::PaintAnswer
ToolboxAttributedTextContext::queryPaintDamage(const loka::app::scene::PaintQuery &query) const
{
  using namespace loka::app::scene;
  if (query.placement != PLACEMENT_ELIGIBLE || query.scope != ToolboxPaintScope())
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (!this->table_.valid() || !this->node_ || !this->node_->props.text_
      || !(this->table_.value() == this->node_->props.text_->get()))
    return PaintAnswer::refused(PAINT_REFUSED_PROPS_UNRECONCILED);
  if (ToolboxPaintIsClippedOut(this->rect_, this->paintRect_, this->deliveredFact()))
    return ToolboxExactPaint(this->paintRect_, false);
  if (!this->presented_.isKnown())
    return PaintAnswer::refused(PAINT_REFUSED_HISTORY_UNKNOWN);
  PaintAnswer answer = ToolboxExactPaint(this->paintRect_, !(this->table_.value() == this->presented_.value()));
  answer.damage.coverage = PAINT_COVERAGE_ERASE_AND_PAINT;
  return answer;
}

void ToolboxAttributedTextContext::render(loka::app::scene::IPlatformController *)
{
  if (!this->controller() || !this->table_.valid() || !this->node_->props.text_
      || !(this->table_.value() == this->node_->props.text_->get()))
    return;
  this->presented_.invalidate();
  ToolboxTextMeasureScope port(*this->controller());
  ToolboxPaintClip clip(this->paintRect_);
  if (!clip.isActive() || !clip.touches(this->paintRect_))
    return;
  const bool painted = this->table_.draw(this->rect_.left,
                                         this->rect_.top,
                                         *this->controller(),
                                         this->node_->props.blockStyle_,
                                         this->rect_.right - this->rect_.left);
  if (painted && clip.covers(this->paintRect_))
    this->presented_.commit(this->table_.value(), ToolboxPaintScope());
}

bool RegisterToolboxAttributedTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  return registry.registerHandler(&handler);
}

void *ToolboxAttributedTextContext::operator new(std::size_t size) throw()
{
  return loka::core::LokaAllocRaw(size, loka::core::LokaAllocationSite("ToolboxAttributedText", "Context"));
}
void ToolboxAttributedTextContext::operator delete(void *storage) throw()
{
  loka::core::LokaFreeRaw(storage, loka::core::LokaAllocationSite("ToolboxAttributedText", "Context"));
}
