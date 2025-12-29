#include "MacTextContext.hpp"
#include <AppKit/AppKit.h>
#include "app/Text.hpp"
#include "core/State.hpp"

namespace
{
  NSString *CreateNSStringFromUtf8(const std::string &value)
  {
    if (value.empty())
    {
      return @"";
    }
    NSString *string = [NSString stringWithUTF8String:value.c_str()];
    if (string)
    {
      return string;
    }
    NSData *data = [NSData dataWithBytes:value.data() length:value.size()];
    string = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return string ? string : @"";
  }
}

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
  [label setStringValue:CreateNSStringFromUtf8(textState_->get())];
}

void MacTextContext::TextChangedThunk(void *userData)
{
  MacTextContext *self = static_cast<MacTextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
