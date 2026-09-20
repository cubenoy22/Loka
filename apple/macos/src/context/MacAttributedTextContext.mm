#include "MacAttributedTextContext.hpp"
#include "../MacScenePlatformController.hpp"
#include "../MacObjCCompat.hpp"
#include "../platform/MacNativeGeometry.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "platform/StringUTF8.hpp"
#include <AppKit/AppKit.h>
#include <cassert>
#include <climits>
#include <new>

namespace
{
  NSLineBreakMode LineBreakMode(const loka::app::BlockStyle &block)
  {
    if (block.hasWrap_ && block.wrap_ != loka::app::TEXT_WRAP_NONE)
      return block.wrap_ == loka::app::TEXT_WRAP_CHAR ? NSLineBreakByCharWrapping : NSLineBreakByWordWrapping;
    return block.hasTruncation_ && block.truncation_ == loka::app::TEXT_TRUNCATION_ELLIPSIS
               ? NSLineBreakByTruncatingTail
               : NSLineBreakByClipping;
  }

  /** Both the measuring cell and the installed cell cross this same door. */
  void ConfigureCell(NSTextFieldCell *cell, const loka::app::BlockStyle &block)
  {
    const BOOL wraps = block.hasWrap_ && block.wrap_ != loka::app::TEXT_WRAP_NONE;
    if ([cell respondsToSelector:@selector(setUsesSingleLineMode:)])
      [cell setUsesSingleLineMode:!wraps];
    [cell setWraps:wraps];
    [cell setScrollable:!wraps];
    [cell setLineBreakMode:LineBreakMode(block)];
  }

  /** Coalesce logical equals before decoding/ranging. NSString lengths are
      UTF-16 units, including two units for supplementary code points. */
  NSString *NextRun(const loka::app::AttributedString &value, std::size_t &index)
  {
    const loka::app::TextStyle &style = value.segment(index).style;
    std::string joined;
    do
    {
      std::string bytes;
      if (!loka::platform::CollectUtf8(value.segment(index).text, bytes))
        return nil;
      joined.append(bytes);
      ++index;
    } while (index < value.segmentCount() && value.segment(index).style == style);
    return [[[NSString alloc] initWithBytes:joined.data() length:joined.size()
                                   encoding:NSUTF8StringEncoding] autorelease];
  }

  class MacAttributedTextNodeHandler : public loka::app::scene::RetainedNodeHandler<MacAttributedTextNodeHandler,
                                                                                    loka::app::AttributedTextNode,
                                                                                    MacAttributedTextContext>
  {
  public:
    static loka::app::AttributedTextNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asAttributedTextNode() : 0;
    }
    static MacAttributedTextContext *create(loka::app::AttributedTextNode *node,
                                            loka::app::scene::IPlatformController *controller,
                                            const loka::app::scene::LayoutState &)
    {
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      return new (std::nothrow) MacAttributedTextContext(mac, mac->projectionParentView(), node);
    }
  };
  MacAttributedTextNodeHandler handler;

  short Coordinate(int value)
  {
    return static_cast<short>(value > SHRT_MAX ? SHRT_MAX : value < SHRT_MIN ? SHRT_MIN : value);
  }
} // namespace

MacAttributedTextContext::Projection::Projection()
    : value_(0)
{
}
MacAttributedTextContext::Projection::~Projection()
{
  this->clear();
}

void MacAttributedTextContext::Projection::clear()
{
  [(NSAttributedString *)this->value_ release];
  this->value_ = 0;
}

bool MacAttributedTextContext::Projection::build(const loka::app::AttributedString &value,
                                                 const loka::app::BlockStyle &block,
                                                 const MacScenePlatformController &controller)
{
  this->clear();
  if (!value.valid())
    return false;
  NSMutableString *joined = [[[NSMutableString alloc] init] autorelease];
  if (!joined)
    return false;
  for (std::size_t index = 0; index < value.segmentCount();)
  {
    NSString *run = NextRun(value, index);
    if (!run)
      return false;
    [joined appendString:run];
  }
  NSMutableParagraphStyle *paragraph = [[[NSMutableParagraphStyle alloc] init] autorelease];
  if (!paragraph)
    return false;
  [paragraph setLineBreakMode:LineBreakMode(block)];
  NSDictionary *defaults = [NSDictionary dictionaryWithObject:paragraph forKey:NSParagraphStyleAttributeName];
  if (!defaults)
    return false;
  NSMutableAttributedString *result = [[[NSMutableAttributedString alloc] initWithString:joined
                                                                              attributes:defaults] autorelease];
  if (!result)
    return false;
  NSUInteger offset = 0;
  for (std::size_t index = 0; index < value.segmentCount();)
  {
    NSFont *font = (NSFont *)controller.textFont(value.segment(index).style);
    NSString *run = NextRun(value, index);
    if (!font || !run)
      return false;
    const NSUInteger length = [run length];
    [result addAttribute:NSFontAttributeName value:font range:NSMakeRange(offset, length)];
    offset += length;
  }
  // The builder never escapes as mutable storage.
  this->value_ = (void *)[result copy];
  return this->value_ != 0;
}

MacAttributedTextContext::MacAttributedTextContext(MacScenePlatformController *controller,
                                                   void *parentView,
                                                   loka::app::AttributedTextNode *node)
    : MacRetirableContext(controller),
      node_(node),
      label_(0)
{
  assert(controller && controller->textShaping() == loka::app::WHOLE_LINE);
  NSTextField *label = [[NSTextField alloc] initWithFrame:NSZeroRect];
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setBezeled:NO];
  [label setDrawsBackground:NO];
  this->label_ = (void *)label;
  if (parentView && label)
    [(NSView *)parentView addSubview:label];
}

MacAttributedTextContext::~MacAttributedTextContext()
{
  assert(!this->label_ && "retirement must queue the native view before reclaim");
  assert(!this->projection_.value() && "retirement must drop derived text before reclaim");
}

void MacAttributedTextContext::clearProjection()
{
  [(NSTextField *)this->label_ setStringValue:@""];
  this->projection_.clear();
}

void MacAttributedTextContext::onPropsApplied()
{
  this->clearProjection();
  this->controller()->requestRelayout();
}

void MacAttributedTextContext::readLifecycleFactOnAttach()
{
  [(NSTextField *)this->label_ setHidden:this->node_->lifecycleFact() != loka::app::scene::NODE_FACT_ATTACHED];
}

void MacAttributedTextContext::onFactChanged(loka::app::scene::NodeLifecycleFact,
                                             loka::app::scene::NodeLifecycleFact next)
{
  [(NSTextField *)this->label_ setHidden:next != loka::app::scene::NODE_FACT_ATTACHED];
  if (next == loka::app::scene::NODE_FACT_RETIRED)
  {
    this->clearProjection();
    [(NSTextField *)this->label_ removeFromSuperview];
    this->retireNativeObject(this->label_);
    this->node_ = 0;
  }
}

short MacAttributedTextContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  this->clearProjection();
  NSTextField *label = (NSTextField *)this->label_;
  state.height = 0;
  if (label && this->node_ && this->node_->props.text_
      && this->projection_.build(this->node_->props.text_->get(), this->node_->props.blockStyle_, *this->controller()))
  {
    NSAttributedString *value = (NSAttributedString *)this->projection_.value();
    NSTextFieldCell *measure = [[[NSTextFieldCell alloc] initTextCell:@""] autorelease];
    if (measure)
    {
      [measure setAttributedStringValue:value];
      ConfigureCell(measure, this->node_->props.blockStyle_);
      [label setAttributedStringValue:value];
      ConfigureCell([label cell], this->node_->props.blockStyle_);
      const loka::macos::MacProjection &projection = this->controller()->projection();
      const NSSize size = [measure cellSizeForBounds:loka::macos::MacMeasurementBounds(
                                                         projection.projectLength(state.x, state.x + state.width))];
      state.height = Coordinate(projection.measurementToLu(size.height));
    }
    else
      this->clearProjection();
  }
  loka::macos::SetMacFrame(
      label,
      this->controller()->projection().projectFrame(loka::core::Frame(state.x, state.y, state.width, state.height)));
  [label setNeedsDisplay:YES];
  return Coordinate(state.y + state.height + loka::app::layout::FallbackControlMetrics::kVerticalSpacing);
}

void RegisterMacAttributedTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&handler);
}
