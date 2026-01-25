#include "MacCellContext.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/Cell.hpp"
#include "core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

class MacCellContext;

@interface LokaCellView : NSView
@property(nonatomic, retain) NSString *text;
@property(nonatomic, assign) MacCellContext *context;
@end

@implementation LokaCellView
- (void)drawRect:(NSRect)dirtyRect
{
  [super drawRect:dirtyRect];
  [[NSColor colorWithCalibratedWhite:0.92 alpha:1.0] setFill];
  NSRectFill(self.bounds);
  [[NSColor colorWithCalibratedWhite:0.45 alpha:1.0] setStroke];
  NSFrameRect(self.bounds);

  if (self.text)
  {
    NSDictionary *attrs = @{
      NSFontAttributeName: [NSFont systemFontOfSize:12.0],
      NSForegroundColorAttributeName: [NSColor blackColor]
    };
    NSSize textSize = [self.text sizeWithAttributes:attrs];
    NSRect bounds = self.bounds;
    NSRect textRect = NSMakeRect(
        bounds.origin.x + (bounds.size.width - textSize.width) * 0.5,
        bounds.origin.y + (bounds.size.height - textSize.height) * 0.5,
        textSize.width,
        textSize.height);
    [self.text drawInRect:textRect withAttributes:attrs];
  }
}

- (void)mouseDown:(NSEvent *)event
{
  if (self.context)
  {
    self.context->handleClick();
  }
  [super mouseDown:event];
}
@end

MacCellContext::MacCellContext(void *parentView, int x, int y, int width, int height, loka::app::CellNode *node)
    : node_(node), view_(0), textState_(0)
{
  NSView *parent = (__bridge NSView *)parentView;
  LokaCellView *view = [[LokaCellView alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  [view setWantsLayer:YES];
  view.context = this;

  if (parent)
  {
    [parent addSubview:view];
  }

  view_ = (__bridge void *)view;
  bindText();
}

MacCellContext::~MacCellContext()
{
  unbindText();
  LokaCellView *view = (__bridge LokaCellView *)view_;
  if (view)
  {
    [view removeFromSuperview];
  }
  view_ = 0;
}

void MacCellContext::handleClick()
{
  if (!node_ || !node_->props.onClick_)
  {
    return;
  }
  node_->props.onClick_->emit();
}

void MacCellContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->deferBind(&MacCellContext::TextChangedThunk, this);
    applyText();
  }
}

void MacCellContext::unbindText()
{
  if (textState_)
  {
    textState_->deferUnbind(&MacCellContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void MacCellContext::applyText()
{
  LokaCellView *view = (__bridge LokaCellView *)view_;
  if (!view || !textState_)
  {
    return;
  }
  std::string utf8;
  if (loka::platform::CollectUtf8(textState_->get(), utf8))
  {
    view.text = loka::macos::CreateNSStringFromUtf8(utf8);
    [view setNeedsDisplay:YES];
  }
}

void MacCellContext::TextChangedThunk(void *userData)
{
  MacCellContext *self = static_cast<MacCellContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
