#include "MacCellContext.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/Cell.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

class MacCellContext;

@interface LokaCellView : NSView
{
  NSString *text_;
  MacCellContext *context_;
}
- (NSString *)text;
- (void)setText:(NSString *)value;
- (MacCellContext *)context;
- (void)setContext:(MacCellContext *)value;
@end

@implementation LokaCellView
- (NSString *)text
{
  return text_;
}

- (void)setText:(NSString *)value
{
  if (text_ != value)
  {
    [text_ release];
    text_ = [value retain];
  }
}

- (MacCellContext *)context
{
  return context_;
}

- (void)setContext:(MacCellContext *)value
{
  context_ = value;
}

- (void)dealloc
{
  [text_ release];
  text_ = nil;
  [super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect
{
  [super drawRect:dirtyRect];
  [[NSColor colorWithCalibratedWhite:0.92 alpha:1.0] setFill];
  NSRectFill(self.bounds);
  [[NSColor colorWithCalibratedWhite:0.45 alpha:1.0] setStroke];
  NSFrameRect(self.bounds);

  if ([self text])
  {
    NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
                                            [NSFont systemFontOfSize:12.0], NSFontAttributeName,
                                            [NSColor blackColor], NSForegroundColorAttributeName,
                                            nil];
    NSSize textSize = [[self text] sizeWithAttributes:attrs];
    NSRect bounds = self.bounds;
    NSRect textRect = NSMakeRect(
        bounds.origin.x + (bounds.size.width - textSize.width) * 0.5,
        bounds.origin.y + (bounds.size.height - textSize.height) * 0.5,
        textSize.width,
        textSize.height);
    [[self text] drawInRect:textRect withAttributes:attrs];
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
  NSView *parent = (NSView *)parentView;
  LokaCellView *view = [[LokaCellView alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  if ([view respondsToSelector:@selector(setWantsLayer:)])
  {
    [view setWantsLayer:YES];
  }
  [view setContext:this];

  if (parent)
  {
    [parent addSubview:view];
  }

  view_ = (void *)view;
  bindText();
}

MacCellContext::~MacCellContext()
{
  unbindText();
  LokaCellView *view = (LokaCellView *)view_;
  if (view)
  {
    [view removeFromSuperview];
  }
  if (view_)
  {
    [(id)view_ release];
  }
  view_ = 0;
}

void MacCellContext::relayout(int x, int y, int width, int height)
{
  LokaCellView *view = (LokaCellView *)view_;
  if (!view)
  {
    return;
  }
  [view setFrame:NSMakeRect(x, y, width, height)];
  [view setNeedsDisplay:YES];
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
  LokaCellView *view = (LokaCellView *)view_;
  if (!view || !textState_)
  {
    return;
  }
  std::string utf8;
  if (loka::platform::CollectUtf8(textState_->get(), utf8))
  {
    [view setText:loka::macos::CreateNSStringFromUtf8(utf8)];
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
