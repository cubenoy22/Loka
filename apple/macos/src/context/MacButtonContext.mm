#include "MacButtonContext.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/Button.hpp"
#include "core/State.hpp"


@interface DeclaraButtonTarget : NSObject
@property(nonatomic, assign) MacButtonContext *owner;
- (IBAction)buttonPressed:(id)sender;
@end

@implementation DeclaraButtonTarget
- (IBAction)buttonPressed:(id)sender
{
  (void)sender;
  if (self.owner)
  {
    self.owner->handlePress();
  }
}
@end

MacButtonContext::MacButtonContext(void *parentView, int x, int y, int width, int height, declara::app::ButtonNode *node)
    : node_(node), button_(0), target_(0), textState_(0)
{
  NSView *parent = (__bridge NSView *)parentView;
  NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  [button setBezelStyle:NSBezelStyleRounded];
  [button setButtonType:NSButtonTypeMomentaryPushIn];

  DeclaraButtonTarget *target = [[DeclaraButtonTarget alloc] init];
  target.owner = this;
  [button setTarget:target];
  [button setAction:@selector(buttonPressed:)];

  if (parent)
  {
    [parent addSubview:button];
  }

  button_ = (__bridge void *)button;
  target_ = (__bridge_retained void *)target;
  bindText();
}

MacButtonContext::~MacButtonContext()
{
  unbindText();
  NSButton *button = (__bridge NSButton *)button_;
  if (button)
  {
    [button setTarget:nil];
    [button setAction:nil];
    [button removeFromSuperview];
  }
  if (target_)
  {
    CFRelease(target_);
    target_ = 0;
  }
  button_ = 0;
}

void MacButtonContext::handlePress()
{
  if (node_ && node_->props.onClick_)
  {
    node_->props.onClick_->emit();
  }
}

void MacButtonContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<State<std::string> *>(node_->props.text_);
  if (textState_)
  {
    textState_->deferBind(&MacButtonContext::TextChangedThunk, this);
    applyText();
  }
}

void MacButtonContext::unbindText()
{
  if (textState_)
  {
    textState_->deferUnbind(&MacButtonContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void MacButtonContext::applyText()
{
  NSButton *button = (__bridge NSButton *)button_;
  if (!button || !textState_)
  {
    return;
  }
  [button setTitle:declara::macos::CreateNSStringFromUtf8(textState_->get())];
}

void MacButtonContext::TextChangedThunk(void *userData)
{
  MacButtonContext *self = static_cast<MacButtonContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
