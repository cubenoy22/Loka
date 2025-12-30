#include "MacTextContext.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/Text.hpp"
#include "core/State.hpp"

MacTextContext::MacTextContext(void *parentView, int x, int y, int width, int height, declara::app::TextNode *node)
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
  textState_ = static_cast<State<std::string> *>(node_->props.text);
  if (textState_)
  {
    textState_->bind(&MacTextContext::TextChangedThunk, this, true);
  }
}

void MacTextContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&MacTextContext::TextChangedThunk, this);
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
  [label setStringValue:declara::macos::CreateNSStringFromUtf8(textState_->get())];
}

void MacTextContext::TextChangedThunk(void *userData)
{
  MacTextContext *self = static_cast<MacTextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
