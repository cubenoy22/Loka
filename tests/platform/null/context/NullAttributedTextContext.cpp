#include "platform/null/context/NullAttributedTextContext.hpp"

#include <climits>
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "platform/null/NullScenePlatformController.hpp"

namespace
{
  int extent(int value)
  {
    return value > SHRT_MAX ? SHRT_MAX : value;
  }

  NullTextMeasurement emptyMeasurement()
  {
    const NullTextMetrics metrics(loka::app::TextStyle(), 0);
    return NullTextMeasurement(0, static_cast<short>(metrics.lineHeight()), 1);
  }

  NullTextMeasurement measure(const loka::app::AttributedString &value,
                              const loka::app::BlockStyle &block,
                              const loka::app::scene::LayoutState &state,
                              bool &materialized)
  {
    materialized = value.valid();
    if (!materialized)
      return emptyMeasurement();
    const NullTextMetrics defaults(loka::app::TextStyle(), 0);
    NullTextLayout layout(defaults.lineHeight());
    for (std::size_t run = 0; run < value.segmentCount(); ++run)
    {
      const loka::app::AttributedString::Segment &segment = value.segment(run);
      const NullTextMetrics metrics(segment.style, &segment.text);
      if (!layout.append(metrics))
      {
        materialized = false;
        return emptyMeasurement();
      }
    }
    return layout.measure(block, state.width);
  }

  bool fits(const loka::app::AttributedString &value, const loka::app::BlockStyle &block, const loka::core::Frame &seat)
  {
    loka::app::scene::LayoutState state;
    state.width = static_cast<short>(seat.width);
    bool materialized = false;
    const NullTextMeasurement output = measure(value, block, state, materialized);
    return materialized && output.width() <= seat.width && output.height() <= seat.height;
  }

  class NullAttributedTextNodeHandler : public loka::app::scene::RetainedNodeHandler<NullAttributedTextNodeHandler,
                                                                                     loka::app::AttributedTextNode,
                                                                                     NullAttributedTextContext>
  {
  public:
    static loka::app::AttributedTextNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asAttributedTextNode() : 0;
    }
    static NullAttributedTextContext *create(loka::app::AttributedTextNode *node,
                                             loka::app::scene::IPlatformController *,
                                             const loka::app::scene::LayoutState &)
    {
      return new NullAttributedTextContext(node);
    }
  };
  NullAttributedTextNodeHandler handler;
} // namespace

NullAttributedTextContext::NullAttributedTextContext(loka::app::AttributedTextNode *node)
    : node_(node)
{
}

short NullAttributedTextContext::layout(loka::app::scene::IPlatformController *controller,
                                        loka::app::scene::LayoutState &state)
{
  bool materialized = false;
  const loka::app::AttributedString empty;
  this->measurement_ = measure(this->node_->props.text_ ? this->node_->props.text_->get() : empty,
                               this->node_->props.blockStyle_,
                               state,
                               materialized);
  state.height = this->measurement_.height();
  this->invalidatePresentation();
  loka::app::scene::PaintScope scope;
  if (materialized && controller
      && static_cast<NullScenePlatformController *>(controller)->queryPaintProjectionScope(scope))
  {
    this->placement_.complete(loka::core::Frame(state.x, state.y, state.width, state.height), scope);
    this->placedBlock_ = this->node_->props.blockStyle_;
  }
  const int bottom = state.y + state.height + state.spacing;
  return static_cast<short>(bottom < SHRT_MIN ? SHRT_MIN : extent(bottom));
}

void NullAttributedTextContext::invalidatePresentation()
{
  this->placement_.invalidate();
  this->presented_.invalidate();
}

void NullAttributedTextContext::onFactChanged(loka::app::scene::NodeLifecycleFact,
                                              loka::app::scene::NodeLifecycleFact next)
{
  if (next != loka::app::scene::NODE_FACT_ATTACHED)
    this->invalidatePresentation();
}

loka::app::scene::PaintAnswer
NullAttributedTextContext::queryPaintDamage(const loka::app::scene::PaintQuery &query) const
{
  using namespace loka::app::scene;
  if (!this->node_->props.text_ || !this->node_->props.text_->get().valid())
    return PaintAnswer::refused(PAINT_REFUSED_PROPS_UNRECONCILED);
  loka::core::Frame seat;
  if (query.placement != PLACEMENT_ELIGIBLE || !this->placement_.query(query.scope, seat)
      || !(this->placedBlock_ == this->node_->props.blockStyle_))
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (!this->presented_.isKnown())
    return PaintAnswer::refused(PAINT_REFUSED_HISTORY_UNKNOWN);
  if (this->presented_.scope() != query.scope)
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  const loka::app::AttributedString &current = this->node_->props.text_->get();
  PaintDamage damage = {query.scope, seat.x, seat.y, 0, 0, PAINT_COVERAGE_ERASE_AND_PAINT};
  if (current == this->presented_.value())
    return PaintAnswer::exact(damage);
  if (!fits(current, this->node_->props.blockStyle_, seat))
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  damage.width = seat.width;
  damage.height = seat.height;
  return PaintAnswer::exact(damage);
}

bool NullAttributedTextContext::commitPresented(const loka::app::AttributedString &value,
                                                const loka::app::scene::PaintScope &scope)
{
  loka::core::Frame seat;
  if (!this->placement_.query(scope, seat) || !fits(value, this->node_->props.blockStyle_, seat))
  {
    this->presented_.invalidate();
    return false;
  }
  this->placedBlock_ = this->node_->props.blockStyle_;
  this->presented_.commit(value, scope);
  return true;
}

void RegisterNullAttributedTextNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&handler);
}
const void *NullAttributedTextNodeHandlerKey()
{
  return handler.nodeTypeKey();
}
bool IsNullAttributedTextNodeHandler(const loka::app::scene::IPlatformNodeHandler *candidate)
{
  return candidate == &handler;
}
