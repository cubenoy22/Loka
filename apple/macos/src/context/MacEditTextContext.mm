#include "MacEditTextContext.hpp"
#include "MacObjCCompat.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/EditText.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

@interface LokaTextFieldDelegate : NSObject
{
  MacEditTextContext *owner_;
}
@property(nonatomic, assign) MacEditTextContext *owner;
@end

@implementation LokaTextFieldDelegate
@synthesize owner = owner_;
- (void)controlTextDidChange:(NSNotification *)notification
{
  (void)notification;
  if (self.owner)
  {
    self.owner->handleTextDidChange();
  }
}
@end

MacEditTextContext::MacEditTextContext(void *parentView, int x, int y, int width, int height, loka::app::EditTextNode *node)
    : node_(node), field_(0), delegate_(0), textState_(0), applyingFromState_(false), updatingFromControl_(false)
{
  NSView *parent = (NSView *)parentView;
  NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  [field setEditable:YES];
  [field setSelectable:YES];
  [field setBezeled:YES];
  [field setDrawsBackground:YES];
  if (node_)
  {
    [field setTag:node_->props.controlTag_];
  }

  LokaTextFieldDelegate *delegate = [[LokaTextFieldDelegate alloc] init];
  delegate.owner = this;
  [field setDelegate:delegate];

  if (parent)
  {
    [parent addSubview:field];
  }

  field_ = (void *)field;
  delegate_ = (void *)delegate;
  bindText();
}

MacEditTextContext::~MacEditTextContext()
{
  unbindText();
  NSTextField *field = (NSTextField *)field_;
  if (field)
  {
    [field setDelegate:nil];
    [field removeFromSuperview];
  }
  if (delegate_)
  {
    [(id)delegate_ release];
    delegate_ = 0;
  }
  if (field_)
  {
    [(id)field_ release];
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

void *MacEditTextContext::nativeField() const
{
  return field_;
}

void MacEditTextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->deferBind(&MacEditTextContext::TextChangedThunk, this);
    applyText();
  }
}

void MacEditTextContext::unbindText()
{
  if (textState_)
  {
    textState_->deferUnbind(&MacEditTextContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void MacEditTextContext::applyText()
{
  NSTextField *field = (NSTextField *)field_;
  if (!field || !textState_)
  {
    return;
  }
  if (updatingFromControl_)
  {
    return;
  }
  std::string desired;
  if (!loka::platform::CollectUtf8(textState_->get(), desired))
  {
    desired.clear();
  }
  std::string current = loka::macos::Utf8FromNSString([field stringValue]);
  if (current == desired)
  {
    return;
  }
  applyingFromState_ = true;
  [field setStringValue:loka::macos::CreateNSStringFromUtf8(desired)];
  applyingFromState_ = false;
}

void MacEditTextContext::syncStateFromControl()
{
  NSTextField *field = (NSTextField *)field_;
  if (!textState_ || !field)
  {
    return;
  }
  loka::core::MutableState<loka::core::String> *mutableState = dynamic_cast<loka::core::MutableState<loka::core::String> *>(textState_);
  if (!mutableState)
  {
    return;
  }
  updatingFromControl_ = true;
  std::string utf8 = loka::macos::Utf8FromNSString([field stringValue]);
  mutableState->set(loka::core::String(utf8), true);
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
