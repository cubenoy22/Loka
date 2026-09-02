#include "MacRectSurfaceContext.hpp"
#include <cassert>
#include "../MacObjCCompat.hpp"
#include "app/RectSurface.hpp"
#include <AppKit/AppKit.h>

namespace
{
  NSColor *MacRectSurfaceClearColor()
  {
    return [NSColor whiteColor];
  }

  NSColor *MacRectSurfaceContentColor()
  {
    return [NSColor blackColor];
  }
} // namespace

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

MacRectSurfaceContext::MacRectSurfaceContext(MacScenePlatformController *controller,
                                             void *parentView,
                                             int x,
                                             int y,
                                             int width,
                                             int height,
                                             loka::app::RectSurfaceNode *node)
    : MacRetirableContext(controller),
      node_(node),
      modelState_(0),
      view_(0)
{
  NSView *parent = (NSView *)parentView;
  LokaRectSurfaceView *view = [[LokaRectSurfaceView alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  [view setContext:this];
  if (parent)
  {
    [parent addSubview:view];
  }
  view_ = view;
  bindModel();
}

MacRectSurfaceContext::~MacRectSurfaceContext()
{
  assert(!view_ && "terminal fact delivery must queue the native view before context reclaim");
}

void MacRectSurfaceContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void MacRectSurfaceContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                          loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (next == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
  else
  {
    // DETACHED_RETAINED hides; terminal RETIRED keeps the same policy
    // (hide before the ritual destroys the native pair).
    this->applyDetachedPresentation();
    if (next == loka::app::scene::NODE_FACT_RETIRED)
    {
      this->unbindModel();
      LokaRectSurfaceView *view = (LokaRectSurfaceView *)this->view_;
      [view setContext:0];
      [view removeFromSuperview];
      this->retireNativeObject(this->view_);
      this->node_ = 0;
    }
  }
}

void MacRectSurfaceContext::applyAttachedPresentation()
{
  LokaRectSurfaceView *view = (LokaRectSurfaceView *)view_;
  if (view)
  {
    [view setHidden:NO];
  }
}

void MacRectSurfaceContext::applyDetachedPresentation()
{
  LokaRectSurfaceView *view = (LokaRectSurfaceView *)view_;
  if (view)
  {
    [view setHidden:YES];
  }
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

void MacRectSurfaceContext::ModelChangedThunk(void *userData)
{
  MacRectSurfaceContext *self = static_cast<MacRectSurfaceContext *>(userData);
  if (self)
  {
    self->applyModel();
  }
}

void MacRectSurfaceContext::bindModel()
{
  if (!node_)
  {
    return;
  }
  modelState_ = node_->props.model_;
  if (modelState_)
  {
    modelState_->deferBind(&MacRectSurfaceContext::ModelChangedThunk, this);
    applyModel();
  }
}

void MacRectSurfaceContext::unbindModel()
{
  if (modelState_)
  {
    modelState_->deferUnbind(&MacRectSurfaceContext::ModelChangedThunk, this);
    modelState_ = 0;
  }
}

void MacRectSurfaceContext::applyModel()
{
  LokaRectSurfaceView *view = (LokaRectSurfaceView *)view_;
  if (!view)
  {
    return;
  }
  [view setNeedsDisplay:YES];
}

void MacRectSurfaceContext::draw(void *viewBounds)
{
  NSRect bounds = *(NSRect *)viewBounds;
  if (node_ && node_->props.clearBackground_)
  {
    [MacRectSurfaceClearColor() setFill];
    NSRectFill(bounds);
  }
  if (!node_ || !node_->props.model_)
  {
    return;
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  [MacRectSurfaceContentColor() setFill];
  for (short i = 0; i < model.spriteCount; ++i)
  {
    const loka::app::RectSurfaceSprite &sprite = model.sprites[i];
    switch (sprite.kind())
    {
    case loka::app::RectSurfaceSprite::KIND_RECT:
      break;
    case loka::app::RectSurfaceSprite::KIND_IMAGE:
      continue;
    }
    NSRectFill(NSMakeRect((CGFloat)sprite.x, (CGFloat)sprite.y, (CGFloat)sprite.width, (CGFloat)sprite.height));
  }
}
