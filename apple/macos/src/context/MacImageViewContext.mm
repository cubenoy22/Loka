#include "MacImageViewContext.hpp"
#include "../MacScenePlatformController.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include <AppKit/AppKit.h>

@interface LokaImageView : NSView
{
  NSImage *image_;
  int fitMode_;
}
- (void)setImage:(NSImage *)image;
- (void)setFitMode:(int)fitMode;
@end

namespace
{
  const int kVerticalSpacing = 12;

  int ResolveImageLayoutWidth(const loka::app::ImageViewNode *node, int fallbackWidth)
  {
    if (!node)
    {
      return fallbackWidth;
    }
    int sizePolicy = loka::app::IMAGE_VIEW_SIZE_AUTO;
    if (node->props.hasAttr_ && node->props.attr_.hasSizePolicyValue_)
    {
      sizePolicy = static_cast<int>(node->props.attr_.sizePolicyValue_);
    }
    const bool hasExplicitWidth = node->props.width_ > 0;
    int srcWidth = 0;
    if (node->props.image_)
    {
      const loka::core::resource::Image current = node->props.image_->get();
      srcWidth = current.width();
    }
    if (hasExplicitWidth)
    {
      return node->props.width_;
    }
    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_INTRINSIC && srcWidth > 0)
    {
      return srcWidth;
    }
    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT)
    {
      return fallbackWidth;
    }
    return fallbackWidth;
  }

  int ResolveImageLayoutHeight(const loka::app::ImageViewNode *node,
                               int resolvedWidth,
                               int fallbackHeight)
  {
    if (!node)
    {
      return fallbackHeight > 0 ? fallbackHeight : 160;
    }
    int sizePolicy = loka::app::IMAGE_VIEW_SIZE_AUTO;
    if (node->props.hasAttr_ && node->props.attr_.hasSizePolicyValue_)
    {
      sizePolicy = static_cast<int>(node->props.attr_.sizePolicyValue_);
    }
    const bool hasExplicitHeight = node->props.height_ > 0;
    int srcWidth = 0;
    int srcHeight = 0;
    if (node->props.image_)
    {
      const loka::core::resource::Image current = node->props.image_->get();
      srcWidth = current.width();
      srcHeight = current.height();
    }
    if (hasExplicitHeight)
    {
      return node->props.height_;
    }
    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && fallbackHeight > 0)
    {
      return fallbackHeight;
    }
    if (srcWidth > 0 && srcHeight > 0 && resolvedWidth > 0)
    {
      return (resolvedWidth * srcHeight) / srcWidth;
    }
    if (srcHeight > 0)
    {
      return srcHeight;
    }
    if (fallbackHeight > 0)
    {
      return fallbackHeight;
    }
    return 160;
  }

  class MacImageViewNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::ImageViewNode *image = node ? node->asImageViewNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!image || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureImageViewContext(image,
                                                          state.x,
                                                          state.y,
                                                          state.width,
                                                          state.height);
    }
  };

  MacImageViewNodeHandler gMacImageViewNodeHandler;
}

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
    if ([image_ respondsToSelector:@selector(drawInRect:fromRect:operation:fraction:respectFlipped:hints:)])
    {
      [image_ drawInRect:dstRect
                fromRect:srcRect
               operation:op
                fraction:1.0
          respectFlipped:YES
                   hints:nil];
    }
    else
    {
      // drawInRect:fromRect:operation:fraction:respectFlipped:hints: is 10.6+.
      // Fall back to the 10.0-compatible variant.
      [image_ drawInRect:dstRect fromRect:srcRect operation:op fraction:1.0];
    }
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

short MacImageViewContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  const int imageWidth = ResolveImageLayoutWidth(this->node_, state.width);
  const int imageHeight = ResolveImageLayoutHeight(this->node_, imageWidth, state.height);
  this->relayout(state.x, state.y, imageWidth, imageHeight);
  state.width = static_cast<short>(imageWidth);
  state.height = static_cast<short>(imageHeight);
  return static_cast<short>(state.y + imageHeight + kVerticalSpacing);
}

void MacImageViewContext::relayout(int x, int y, int width, int height)
{
  LokaImageView *view = (LokaImageView *)imageView_;
  if (!view)
  {
    return;
  }
  [view setFrame:NSMakeRect(x, y, width, height)];
  [view setNeedsDisplay:YES];
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

void RegisterMacImageViewNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gMacImageViewNodeHandler);
}
