#include "MacRectSurfaceContext.hpp"
#include "app/RectSurface.hpp"
#include <AppKit/AppKit.h>

@interface LokaRectSurfaceView : NSView
{
  MacRectSurfaceContext *context_;
}
- (void)setContext:(MacRectSurfaceContext *)value;
@end

@implementation LokaRectSurfaceView
- (BOOL)isFlipped
{
  return YES;
}

- (void)setContext:(MacRectSurfaceContext *)value
{
  context_ = value;
}

- (void)drawRect:(NSRect)dirtyRect
{
  (void)dirtyRect;
  if (context_)
  {
    NSRect bounds = [self bounds];
    context_->draw(&bounds);
  }
}
@end

MacRectSurfaceContext::MacRectSurfaceContext(void *parentView,
                                             int x,
                                             int y,
                                             int width,
                                             int height,
                                             loka::app::RectSurfaceNode *node)
    : node_(node),
      view_(0)
{
  NSView *parent = (NSView *)parentView;
  LokaRectSurfaceView *view =
      [[LokaRectSurfaceView alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  [view setContext:this];
  if (parent)
  {
    [parent addSubview:view];
  }
  view_ = view;
}

MacRectSurfaceContext::~MacRectSurfaceContext()
{
  LokaRectSurfaceView *view = (LokaRectSurfaceView *)view_;
  if (view)
  {
    [view removeFromSuperview];
    [view release];
  }
  view_ = 0;
}

void MacRectSurfaceContext::relayout(int x, int y, int width, int height)
{
  LokaRectSurfaceView *view = (LokaRectSurfaceView *)view_;
  if (!view)
  {
    return;
  }
  [view setFrame:NSMakeRect(x, y, width, height)];
  [view setNeedsDisplay:YES];
}

void MacRectSurfaceContext::draw(void *viewBounds)
{
  NSRect bounds = *(NSRect *)viewBounds;
  if (node_ && node_->props.clearBackground_)
  {
    [[NSColor colorWithCalibratedRed:0.85 green:0.93 blue:1.0 alpha:1.0] setFill];
    NSRectFill(bounds);
  }
  if (!node_ || !node_->props.model_)
  {
    return;
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  [[NSColor blackColor] setFill];
  for (short i = 0; i < model.rectCount; ++i)
  {
    NSRectFill(NSMakeRect((CGFloat)model.rects[i].x,
                          (CGFloat)model.rects[i].y,
                          (CGFloat)model.rects[i].width,
                          (CGFloat)model.rects[i].height));
  }
}
