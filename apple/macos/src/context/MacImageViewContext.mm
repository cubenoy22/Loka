#include "MacImageViewContext.hpp"
#include <AppKit/AppKit.h>

@interface LokaImageView : NSView
{
  NSImage *image_;
  int fitMode_;
}
- (void)setImage:(NSImage *)image;
- (void)setFitMode:(int)fitMode;
@end

@implementation LokaImageView
- (id)initWithFrame:(NSRect)frame
{
  self = [super initWithFrame:frame];
  if (self)
  {
    image_ = 0;
    fitMode_ = loka::app::IMAGE_FIT_STRETCH;
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

- (void)setFitMode:(int)fitMode
{
  if (fitMode_ == fitMode)
  {
    return;
  }
  fitMode_ = fitMode;
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
  (void)dirtyRect;
  [[NSColor colorWithCalibratedWhite:0.94 alpha:1.0] setFill];
  NSRectFill([self bounds]);
  if (image_)
  {
    NSRect bounds = [self bounds];
    NSRect dstRect = bounds;
    NSRect srcRect = NSZeroRect;
    const NSSize imageSize = [image_ size];
    const CGFloat srcW = imageSize.width;
    const CGFloat srcH = imageSize.height;
    const CGFloat dstW = bounds.size.width;
    const CGFloat dstH = bounds.size.height;

    if (srcW > 0 && srcH > 0 && dstW > 0 && dstH > 0)
    {
      if (fitMode_ == loka::app::IMAGE_FIT_NONE)
      {
        dstRect.size.width = srcW;
        dstRect.size.height = srcH;
      }
      else if (fitMode_ == loka::app::IMAGE_FIT_CONTAIN)
      {
        const CGFloat srcAspect = srcW / srcH;
        const CGFloat dstAspect = dstW / dstH;
        if (srcAspect > dstAspect)
        {
          dstRect.size.height = dstW / srcAspect;
        }
        else
        {
          dstRect.size.width = dstH * srcAspect;
        }
        dstRect.origin.x = bounds.origin.x + (dstW - dstRect.size.width) * 0.5f;
        dstRect.origin.y = bounds.origin.y + (dstH - dstRect.size.height) * 0.5f;
      }
      else if (fitMode_ == loka::app::IMAGE_FIT_COVER)
      {
        srcRect = NSMakeRect(0, 0, srcW, srcH);
        const CGFloat srcAspect = srcW / srcH;
        const CGFloat dstAspect = dstW / dstH;
        if (srcAspect > dstAspect)
        {
          const CGFloat cropW = srcH * dstAspect;
          srcRect.origin.x = (srcW - cropW) * 0.5f;
          srcRect.size.width = cropW;
        }
        else
        {
          const CGFloat cropH = srcW / dstAspect;
          srcRect.origin.y = (srcH - cropH) * 0.5f;
          srcRect.size.height = cropH;
        }
      }
    }

#if defined(NSCompositingOperationSourceOver)
    NSCompositingOperation op = NSCompositingOperationSourceOver;
#else
    NSCompositingOperation op = NSCompositeSourceOver;
#endif
    [image_ drawInRect:dstRect
              fromRect:srcRect
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
  int fitMode = loka::app::IMAGE_FIT_STRETCH;
  if (node_ && node_->props.hasAttr_ && node_->props.attr_.hasFitValue_)
  {
    fitMode = static_cast<int>(node_->props.attr_.fitValue_);
  }
  [view setFitMode:fitMode];
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
