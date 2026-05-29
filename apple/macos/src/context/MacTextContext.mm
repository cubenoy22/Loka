#include "MacTextContext.hpp"
#include "../MacScenePlatformController.hpp"
#include "../MacObjCCompat.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/nodes/Text.hpp"
#include "loka/core/State.hpp"
#include "platform/StringUTF8.hpp"
#include "core/resource/Image.hpp"

namespace
{
  const int kDefaultTextHeight = 20;
  const int kVerticalSpacing = 12;

  int MeasureTextHeightForWidth(const loka::app::TextNode *text,
                                int width,
                                int defaultHeight)
  {
    if (!text || !text->props.text_)
    {
      return defaultHeight;
    }
    if (!text->props.hasAttr_ || !text->props.attr_.hasWrapValue_ ||
        text->props.attr_.wrapValue_ == loka::app::TEXT_WRAP_NONE)
    {
      return defaultHeight;
    }
    if (width <= 0)
    {
      return defaultHeight;
    }

    std::string utf8;
    if (!loka::platform::CollectUtf8(text->props.text_->get(), utf8))
    {
      return defaultHeight;
    }
    if (utf8.empty())
    {
      return defaultHeight;
    }

    NSString *string = [NSString stringWithUTF8String:utf8.c_str()];
    if (!string)
    {
      return defaultHeight;
    }
    NSFont *font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    int measured = defaultHeight;
    NSTextFieldCell *cell = [[[NSTextFieldCell alloc] initTextCell:string] autorelease];
    if (cell)
    {
      [cell setFont:font];
      [cell setWraps:YES];
      [cell setScrollable:NO];
      [cell setLineBreakMode:NSLineBreakByWordWrapping];
      NSSize size = [cell cellSizeForBounds:NSMakeRect(0.0f, 0.0f,
                                                       static_cast<CGFloat>(width),
                                                       CGFLOAT_MAX)];
      measured = static_cast<int>(size.height + 0.5f);
    }
    const int measuredWithPadding = measured + 2;
    if (measuredWithPadding > defaultHeight)
    {
      return measuredWithPadding;
    }
    return defaultHeight;
  }

  static void SetUsesSingleLineModeCompat(NSTextField *label, BOOL value)
  {
    if (!label)
    {
      return;
    }
    if ([label respondsToSelector:@selector(setUsesSingleLineMode:)])
    {
      [label setUsesSingleLineMode:value];
    }
  }

  static void ReleaseCapturedBitmap(void *handle, void *)
  {
    NSBitmapImageRep *bitmap = (NSBitmapImageRep *)handle;
    if (bitmap)
    {
      [bitmap release];
    }
  }

  static bool CaptureViewBitmap(NSView *view, loka::core::resource::Image &out)
  {
    if (!view)
    {
      return false;
    }
    NSRect bounds = [view bounds];
    if (bounds.size.width <= 0 || bounds.size.height <= 0)
    {
      return false;
    }
    NSBitmapImageRep *bitmap = [view bitmapImageRepForCachingDisplayInRect:bounds];
    if (!bitmap)
    {
      return false;
    }
    [bitmap retain];
    [view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
    out = loka::core::resource::Image::FromNative((void *)bitmap,
                                                  (int)bounds.size.width,
                                                  (int)bounds.size.height,
                                                  &ReleaseCapturedBitmap,
                                                  0);
    if (!out.isValid())
    {
      [bitmap release];
      return false;
    }
    return true;
  }

  class MacTextNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::TextNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::TextNode *text = node ? node->asTextNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!text || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureTextContext(text,
                                                     state.x,
                                                     state.y,
                                                     state.width,
                                                     state.height);
    }
  };

  MacTextNodeHandler gMacTextNodeHandler;
}

MacTextContext::MacTextContext(void *parentView, int x, int y, int width, int height, loka::app::TextNode *node)
    : node_(node),
      parentView_(parentView),
      label_(0),
      textState_(0),
      textStateBound_(false),
      didInitialApply_(false)
{
  NSView *parent = (NSView *)parentView;
  NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setBezeled:NO];
  [label setDrawsBackground:NO];
  if (node_ && node_->props.hasAttr_)
  {
    NSTextFieldCell *cell = [label cell];
    const loka::app::TextAttr &attr = node_->props.attr_;
    const bool wrapWord = attr.hasWrapValue_ && attr.wrapValue_ == loka::app::TEXT_WRAP_WORD;
    const bool wrapChar = attr.hasWrapValue_ && attr.wrapValue_ == loka::app::TEXT_WRAP_CHAR;
    if (wrapWord || wrapChar)
    {
      SetUsesSingleLineModeCompat(label, NO);
      [cell setWraps:YES];
      [cell setScrollable:NO];
      [cell setLineBreakMode:wrapChar ? NSLineBreakByCharWrapping : NSLineBreakByWordWrapping];
    }
    else
    {
      SetUsesSingleLineModeCompat(label, YES);
      [cell setWraps:NO];
      [cell setScrollable:YES];
      NSLineBreakMode mode = NSLineBreakByClipping;
      if (attr.hasTruncationValue_)
      {
        if (attr.truncationValue_ == loka::app::TEXT_TRUNCATION_ELLIPSIS)
        {
          mode = NSLineBreakByTruncatingTail;
        }
        else if (attr.truncationValue_ == loka::app::TEXT_TRUNCATION_CLIP)
        {
          mode = NSLineBreakByClipping;
        }
      }
      [cell setLineBreakMode:mode];
    }
  }

  if (parent)
  {
    [parent addSubview:label];
  }

  label_ = (void *)label;
  bindText();
}

MacTextContext::~MacTextContext()
{
  unbindText();
  NSTextField *label = (NSTextField *)label_;
  if (label)
  {
    [label removeFromSuperview];
  }
  if (label_)
  {
    [(id)label_ release];
  }
  label_ = 0;
}

void MacTextContext::onNodeAttached()
{
  NSTextField *label = (NSTextField *)label_;
  if (label)
  {
    [label setHidden:NO];
  }
}

void MacTextContext::onNodeDetached()
{
  NSTextField *label = (NSTextField *)label_;
  if (label)
  {
    [label setHidden:YES];
  }
}

bool MacTextContext::captureBitmap(loka::core::resource::Image &out) const
{
  return CaptureViewBitmap((NSView *)label_, out);
}

short MacTextContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  const int textHeight = MeasureTextHeightForWidth(this->node_, state.width, kDefaultTextHeight);
  this->relayout(state.x, state.y, state.width, textHeight);
  state.height = static_cast<short>(textHeight);
  return static_cast<short>(state.y + textHeight + kVerticalSpacing);
}

void MacTextContext::relayout(int x, int y, int width, int height)
{
  NSTextField *label = (NSTextField *)label_;
  if (!label)
  {
    return;
  }
  [label setFrame:NSMakeRect(x, y, width, height)];
  [label setNeedsDisplay:YES];
}

void MacTextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  textStateBound_ = false;
  if (textState_)
  {
    if (!node_->props.ownsText)
    {
      textState_->deferBind(&MacTextContext::TextChangedThunk, this);
      textStateBound_ = true;
    }
    applyText();
  }
}

void MacTextContext::unbindText()
{
  if (textState_)
  {
    if (textStateBound_)
    {
      textState_->deferUnbind(&MacTextContext::TextChangedThunk, this);
    }
    textState_ = 0;
    textStateBound_ = false;
  }
}

void MacTextContext::applyText()
{
  NSTextField *label = (NSTextField *)label_;
  if (!label || !textState_)
  {
    return;
  }
  std::string utf8;
  if (loka::platform::CollectUtf8(textState_->get(), utf8))
  {
    [label setStringValue:loka::macos::CreateNSStringFromUtf8(utf8)];
  }
  requestRelayoutIfNeeded();
  if (!didInitialApply_)
  {
    didInitialApply_ = true;
  }
}

void MacTextContext::requestRelayoutIfNeeded()
{
  if (!didInitialApply_ || !node_ || !node_->props.hasAttr_ || !node_->props.attr_.hasWrapValue_)
  {
    return;
  }
  if (node_->props.attr_.wrapValue_ == loka::app::TEXT_WRAP_NONE)
  {
    return;
  }
  NSView *view = (NSView *)parentView_;
  if (!view)
  {
    return;
  }
  MacScenePlatformController *controller = MacScenePlatformController::findForRootView((void *)view);
  if (!controller)
  {
    return;
  }
  // Defer relayout to app flush tick to avoid re-entering layout on notify stack.
  controller->requestRelayout();
}

void MacTextContext::TextChangedThunk(void *userData)
{
  MacTextContext *self = static_cast<MacTextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}

void RegisterMacTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gMacTextNodeHandler);
}
