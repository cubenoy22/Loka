#include "MacImageViewContext.hpp"
#include <AppKit/AppKit.h>

@interface LokaImageView : NSView
{
  NSImage *image_;
}
- (void)setImage:(NSImage *)image;
@end

@implementation LokaImageView
- (id)initWithFrame:(NSRect)frame
{
  self = [super initWithFrame:frame];
  if (self)
  {
    image_ = 0;
  }
  return self;
}

- (void)dealloc
{
  if (image_)
  {
    [image_ release];
    image_ = 0;
  }
  [super dealloc];
}

- (void)setImage:(NSImage *)image
{
  if (image_ == image)
  {
    return;
  }
  if (image_)
  {
    [image_ release];
  }
  image_ = [image retain];
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
  (void)dirtyRect;
  [[NSColor colorWithCalibratedWhite:0.94 alpha:1.0] setFill];
  NSRectFill([self bounds]);
  if (image_)
  {
#if defined(NSCompositingOperationSourceOver)
    NSCompositingOperation op = NSCompositingOperationSourceOver;
#else
    NSCompositingOperation op = NSCompositeSourceOver;
#endif
    [image_ drawInRect:[self bounds]
              fromRect:NSZeroRect
             operation:op
              fraction:1.0
        respectFlipped:YES
                 hints:nil];
  }
}
@end

MacImageViewContext::MacImageViewContext(void *parentView, int x, int y, int width, int height, loka::app::ImageViewNode *node)
    : node_(node), imageView_(0), imageState_(0), image_()
{
  NSView *parent = (NSView *)parentView;
  LokaImageView *view = [[LokaImageView alloc] initWithFrame:NSMakeRect(x, y, width, height)];
  if (parent)
  {
    [parent addSubview:view];
  }
  imageView_ = (void *)view;
  bindImage();
}

MacImageViewContext::~MacImageViewContext()
{
  unbindImage();
  LokaImageView *view = (LokaImageView *)imageView_;
  if (view)
  {
    [view removeFromSuperview];
  }
  if (imageView_)
  {
    [(id)imageView_ release];
  }
  imageView_ = 0;
}

void MacImageViewContext::bindImage()
{
  if (!node_)
  {
    return;
  }
  imageState_ = node_->props.image_;
  if (imageState_)
  {
    imageState_->deferBind(&MacImageViewContext::ImageChangedThunk, this);
    applyImage();
  }
}

void MacImageViewContext::unbindImage()
{
  if (imageState_)
  {
    imageState_->deferUnbind(&MacImageViewContext::ImageChangedThunk, this);
    imageState_ = 0;
  }
}

void MacImageViewContext::applyImage()
{
  LokaImageView *view = (LokaImageView *)imageView_;
  if (!view || !imageState_)
  {
    return;
  }
  image_ = imageState_->get();
  NSImage *image = (NSImage *)image_.nativeHandle();
  [view setImage:image];
}

void MacImageViewContext::ImageChangedThunk(void *userData)
{
  MacImageViewContext *self = static_cast<MacImageViewContext *>(userData);
  if (self)
  {
    self->applyImage();
  }
}
