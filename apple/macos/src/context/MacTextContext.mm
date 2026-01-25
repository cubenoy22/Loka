#include "MacTextContext.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/Text.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

MacTextContext::MacTextContext(void *parentView, int x, int y, int width, int height, loka::app::TextNode *node)
    : node_(node), label_(0), textState_(0)
{
  NSView *parent = (__bridge NSView *)parentView;
  NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setBezeled:NO];
  [label setDrawsBackground:NO];

  if (parent)
  {
    [parent addSubview:label];
  }

  label_ = (__bridge void *)label;
  bindText();
}

MacTextContext::~MacTextContext()
{
  unbindText();
  NSTextField *label = (__bridge NSTextField *)label_;
  if (label)
  {
    [label removeFromSuperview];
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
  NSTextField *label = (__bridge NSTextField *)label_;
  if (!label || !textState_)
  {
    return;
  }
  std::string utf8;
  if (loka::platform::CollectUtf8(textState_->get(), utf8))
  {
    [label setStringValue:loka::macos::CreateNSStringFromUtf8(utf8)];
  }
}

void MacTextContext::TextChangedThunk(void *userData)
{
  MacTextContext *self = static_cast<MacTextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
