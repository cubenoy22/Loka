#include "MacEditTextContext.hpp"
#include <AppKit/AppKit.h>
#include "app/EditText.hpp"
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

  std::string Utf8FromNSString(NSString *string)
  {
    if (!string)
    {
      return std::string();
    }
    const char *bytes = [string UTF8String];
    if (!bytes)
    {
      return std::string();
    }
    return std::string(bytes);
  }
}

@interface DeclaraTextFieldDelegate : NSObject <NSTextFieldDelegate>
@property(nonatomic, assign) MacEditTextContext *owner;
@end

@implementation DeclaraTextFieldDelegate
- (void)controlTextDidChange:(NSNotification *)notification
{
  (void)notification;
  if (self.owner)
  {
    self.owner->handleTextDidChange();
  }
}
@end

MacEditTextContext::MacEditTextContext(void *parentView, int x, int y, int width, int height, declara::app::EditTextNode *node)
    : node_(node), field_(0), delegate_(0), textState_(0), applyingFromState_(false), updatingFromControl_(false)
{
  NSView *parent = (__bridge NSView *)parentView;
  NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  [field setEditable:YES];
  [field setSelectable:YES];
  [field setBezeled:YES];
  [field setDrawsBackground:YES];

  DeclaraTextFieldDelegate *delegate = [[DeclaraTextFieldDelegate alloc] init];
  delegate.owner = this;
  [field setDelegate:delegate];

  if (parent)
  {
    [parent addSubview:field];
  }

  field_ = (__bridge void *)field;
  delegate_ = (__bridge_retained void *)delegate;
  bindText();
}

MacEditTextContext::~MacEditTextContext()
{
  unbindText();
  NSTextField *field = (__bridge NSTextField *)field_;
  if (field)
  {
    [field setDelegate:nil];
    [field removeFromSuperview];
  }
  if (delegate_)
  {
    CFRelease(delegate_);
    delegate_ = 0;
  }
  field_ = 0;
}

void MacEditTextContext::handleTextDidChange()
{
  if (!applyingFromState_)
  {
    syncStateFromControl();
  }
}

void MacEditTextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<State<std::string> *>(node_->props.text);
  if (textState_)
  {
    textState_->bind(&MacEditTextContext::TextChangedThunk, this, true);
  }
}

void MacEditTextContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&MacEditTextContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void MacEditTextContext::applyText()
{
  NSTextField *field = (__bridge NSTextField *)field_;
  if (!field || !textState_)
  {
    return;
  }
  if (updatingFromControl_)
  {
    return;
  }
  const std::string &desired = textState_->get();
  std::string current = Utf8FromNSString([field stringValue]);
  if (current == desired)
  {
    return;
  }
  applyingFromState_ = true;
  [field setStringValue:CreateNSStringFromUtf8(desired)];
  applyingFromState_ = false;
}

void MacEditTextContext::syncStateFromControl()
{
  NSTextField *field = (__bridge NSTextField *)field_;
  if (!textState_ || !field)
  {
    return;
  }
  MutableState<std::string> *mutableState = dynamic_cast<MutableState<std::string> *>(textState_);
  if (!mutableState)
  {
    return;
  }
  updatingFromControl_ = true;
  mutableState->set(Utf8FromNSString([field stringValue]), true);
  updatingFromControl_ = false;
}

void MacEditTextContext::TextChangedThunk(void *userData)
{
  MacEditTextContext *self = static_cast<MacEditTextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
