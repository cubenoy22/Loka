#include "MacButtonContext.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/Button.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "core/resource/Image.hpp"

namespace
{
  static void ReleaseCapturedButtonBitmap(void *handle, void *)
  {
    NSBitmapImageRep *bitmap = (NSBitmapImageRep *)handle;
    if (bitmap)
    {
      [bitmap release];
    }
  }

  static bool CaptureButtonBitmap(NSView *view, loka::core::resource::Image &out)
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
                                                  &ReleaseCapturedButtonBitmap,
                                                  0);
    if (!out.isValid())
    {
      [bitmap release];
      return false;
    }
    return true;
  }
}


@interface LokaButtonTarget : NSObject
{
  MacButtonContext *owner_;
}
@property(nonatomic, assign) MacButtonContext *owner;
- (IBAction)buttonPressed:(id)sender;
@end

@implementation LokaButtonTarget
@synthesize owner = owner_;
- (IBAction)buttonPressed:(id)sender
{
  (void)sender;
  if (self.owner)
  {
    self.owner->handlePress();
  }
}
@end

MacButtonContext::MacButtonContext(void *parentView, int x, int y, int width, int height, loka::app::ButtonNode *node)
    : node_(node), button_(0), target_(0), textState_(0), enabledState_(0)
{
  NSView *parent = (NSView *)parentView;
  NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, width, height)];
#if defined(NSBezelStyleRounded)
  [button setBezelStyle:NSBezelStyleRounded];
#else
  [button setBezelStyle:NSRoundedBezelStyle];
#endif
#if defined(NSButtonTypeMomentaryPushIn)
  [button setButtonType:NSButtonTypeMomentaryPushIn];
#else
  [button setButtonType:NSMomentaryPushInButton];
#endif

  LokaButtonTarget *target = [[LokaButtonTarget alloc] init];
  target.owner = this;
  [button setTarget:target];
  [button setAction:@selector(buttonPressed:)];

  if (parent)
  {
    [parent addSubview:button];
  }

  button_ = (void *)button;
  target_ = (void *)target;
  bindText();
  bindEnabled();
}

MacButtonContext::~MacButtonContext()
{
  unbindText();
  unbindEnabled();
  NSButton *button = (NSButton *)button_;
  if (button)
  {
    [button setTarget:nil];
    [button setAction:nil];
    [button removeFromSuperview];
  }
  if (target_)
  {
    [(id)target_ release];
    target_ = 0;
  }
  if (button_)
  {
    [(id)button_ release];
  }
  button_ = 0;
}

bool MacButtonContext::captureBitmap(loka::core::resource::Image &out) const
{
  return CaptureButtonBitmap((NSView *)button_, out);
}

void MacButtonContext::handlePress()
{
  if (node_ && node_->props.onClick_)
  {
    node_->props.onClick_->emit();
  }
}

void MacButtonContext::relayout(int x, int y, int width, int height)
{
  NSButton *button = (NSButton *)button_;
  if (!button)
  {
    return;
  }
  [button setFrame:NSMakeRect(x, y, width, height)];
}

void MacButtonContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
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

void MacButtonContext::bindEnabled()
{
  if (!node_)
  {
    return;
  }
  enabledState_ = static_cast<loka::core::State<bool> *>(node_->props.enabled_);
  if (enabledState_)
  {
    enabledState_->deferBind(&MacButtonContext::EnabledChangedThunk, this);
    applyEnabled();
  }
}

void MacButtonContext::unbindEnabled()
{
  if (enabledState_)
  {
    enabledState_->deferUnbind(&MacButtonContext::EnabledChangedThunk, this);
    enabledState_ = 0;
  }
}

void MacButtonContext::applyText()
{
  NSButton *button = (NSButton *)button_;
  if (!button || !textState_)
  {
    return;
  }
  std::string utf8;
  if (loka::platform::CollectUtf8(textState_->get(), utf8))
  {
    [button setTitle:loka::macos::CreateNSStringFromUtf8(utf8)];
  }
}

void MacButtonContext::applyEnabled()
{
  NSButton *button = (NSButton *)button_;
  if (!button || !enabledState_)
  {
    return;
  }
  [button setEnabled:enabledState_->get() ? YES : NO];
}

void MacButtonContext::TextChangedThunk(void *userData)
{
  MacButtonContext *self = static_cast<MacButtonContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}

void MacButtonContext::EnabledChangedThunk(void *userData)
{
  MacButtonContext *self = static_cast<MacButtonContext *>(userData);
  if (self)
  {
    self->applyEnabled();
  }
}
