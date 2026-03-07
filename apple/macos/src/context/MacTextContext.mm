#include "MacTextContext.hpp"
#include "../MacScenePlatformController.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/Text.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

MacTextContext::MacTextContext(void *parentView, int x, int y, int width, int height, loka::app::TextNode *node)
    : node_(node), parentView_(parentView), label_(0), textState_(0), didInitialApply_(false)
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
      [label setUsesSingleLineMode:NO];
      [cell setWraps:YES];
      [cell setScrollable:NO];
      [cell setLineBreakMode:wrapChar ? NSLineBreakByCharWrapping : NSLineBreakByWordWrapping];
    }
    else
    {
      [label setUsesSingleLineMode:YES];
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

void MacTextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->deferBind(&MacTextContext::TextChangedThunk, this);
    applyText();
  }
}

void MacTextContext::unbindText()
{
  if (textState_)
  {
    textState_->deferUnbind(&MacTextContext::TextChangedThunk, this);
    textState_ = 0;
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
  if (!parentView_)
  {
    return;
  }
  MacScenePlatformController *controller = MacScenePlatformController::findForRootView(parentView_);
  if (!controller)
  {
    return;
  }
  NSView *view = (NSView *)parentView_;
  NSRect bounds = [view bounds];
  controller->relayout(static_cast<int>(bounds.size.width), static_cast<int>(bounds.size.height));
}

void MacTextContext::TextChangedThunk(void *userData)
{
  MacTextContext *self = static_cast<MacTextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
