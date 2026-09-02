#include "MacRectSurfaceContext.hpp"
#include <cassert>
#include "../MacObjCCompat.hpp"
#include "app/RectSurface.hpp"
#include <AppKit/AppKit.h>
#include <cstring>

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

  unsigned char UnpremultiplyRectSurfaceComponent(unsigned char component, unsigned char alpha)
  {
    const unsigned int value = (static_cast<unsigned int>(component) * 255u + alpha / 2u) / alpha;
    return static_cast<unsigned char>(value > 255u ? 255u : value);
  }

  NSImage *CreateRectSurfaceBinaryAlphaImage(NSImage *source, int sourceWidth, int sourceHeight)
  {
    const NSInteger width = sourceWidth;
    const NSInteger height = sourceHeight;
    if (!source || width <= 0 || height <= 0)
    {
      return 0;
    }
    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:0
                      pixelsWide:width
                      pixelsHigh:height
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:width * 4
                    bitsPerPixel:32];
    if (!bitmap)
    {
      return 0;
    }
    NSGraphicsContext *bitmapContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    if (!bitmapContext)
    {
      [bitmap release];
      return 0;
    }
    std::memset([bitmap bitmapData], 0, static_cast<std::size_t>([bitmap bytesPerRow]) * height);
    NSGraphicsContext *previousContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:bitmapContext];
    [source drawInRect:NSMakeRect(0, 0, width, height)
              fromRect:NSZeroRect
             operation:LOKA_MAC_COMPOSITING_SOURCE_OVER
              fraction:1.0];
    [NSGraphicsContext setCurrentContext:previousContext];

    unsigned char *pixels = [bitmap bitmapData];
    const NSInteger bytesPerRow = [bitmap bytesPerRow];
    const BOOL alphaFirst = ([bitmap bitmapFormat] & NSAlphaFirstBitmapFormat) != 0;
    const BOOL nonpremultiplied = ([bitmap bitmapFormat] & NSAlphaNonpremultipliedBitmapFormat) != 0;
    const NSInteger alphaIndex = alphaFirst ? 0 : 3;
    for (NSInteger y = 0; y < height; ++y)
    {
      unsigned char *row = pixels + y * bytesPerRow;
      for (NSInteger x = 0; x < width; ++x)
      {
        unsigned char *pixel = row + x * 4;
        const unsigned char alpha = pixel[alphaIndex];
        if (alpha < 128)
        {
          pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
          continue;
        }
        if (!nonpremultiplied && alpha < 255)
        {
          for (NSInteger component = 0; component < 4; ++component)
          {
            if (component != alphaIndex)
            {
              pixel[component] = UnpremultiplyRectSurfaceComponent(pixel[component], alpha);
            }
          }
        }
        pixel[alphaIndex] = 255;
      }
    }

    NSImage *prepared = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    if (prepared)
    {
      [prepared addRepresentation:bitmap];
    }
    [bitmap release];
    return prepared;
  }
} // namespace

MacRectSurfacePreparedImage::MacRectSurfacePreparedImage()
    : source_(),
      prepared_(0)
{
}

MacRectSurfacePreparedImage::~MacRectSurfacePreparedImage()
{
  clear();
}

void *MacRectSurfacePreparedImage::prepare(const loka::core::resource::Image &source)
{
  if (prepared_ && source_ == source)
  {
    return prepared_;
  }
  clear();
  if (!source.isValid())
  {
    return 0;
  }
  NSImage *prepared = CreateRectSurfaceBinaryAlphaImage(
      (NSImage *)source.nativeHandle(), source.width(), source.height());
  if (!prepared)
  {
    return 0;
  }
  source_ = source;
  prepared_ = prepared;
  return prepared_;
}

void MacRectSurfacePreparedImage::discardUnless(const loka::core::resource::Image &source)
{
  if (source_ != source)
  {
    clear();
  }
}

void MacRectSurfacePreparedImage::clear()
{
  if (prepared_)
  {
    [(NSImage *)prepared_ release];
    prepared_ = 0;
  }
  source_ = loka::core::resource::Image::Empty();
}

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
      this->clearPreparedImages();
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
  discardStalePreparedImages();
  LokaRectSurfaceView *view = (LokaRectSurfaceView *)view_;
  if (!view)
  {
    return;
  }
  [view setNeedsDisplay:YES];
}

void MacRectSurfaceContext::discardStalePreparedImages()
{
  if (!modelState_)
  {
    clearPreparedImages();
    return;
  }
  const loka::app::RectSurfaceModel model = modelState_->get();
  const short count = model.spriteCount();
  for (short i = 0; i < loka::app::RectSurfaceModel::kMaxSprites; ++i)
  {
    loka::core::resource::Image current;
    if (i < count && model.sprites[i].kind() == loka::app::RectSurfaceSprite::KIND_IMAGE)
    {
      model.sprites[i].queryImage(current);
    }
    preparedImages_[i].discardUnless(current);
  }
}

void MacRectSurfaceContext::clearPreparedImages()
{
  for (short i = 0; i < loka::app::RectSurfaceModel::kMaxSprites; ++i)
  {
    preparedImages_[i].clear();
  }
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
  const loka::app::RectSurfacePaintList paintList(model);
  [MacRectSurfaceContentColor() setFill];
  for (short i = 0; i < paintList.count(); ++i)
  {
    const loka::app::RectSurfaceSprite &sprite = *paintList.querySprite(i);
    switch (sprite.kind())
    {
    case loka::app::RectSurfaceSprite::KIND_RECT:
      break;
    case loka::app::RectSurfaceSprite::KIND_IMAGE:
    {
      loka::core::resource::Image image;
      if (sprite.queryImage(image) && image.isValid())
      {
        NSImage *prepared = (NSImage *)preparedImages_[i].prepare(image);
        if (prepared)
        {
          [prepared drawInRect:NSMakeRect((CGFloat)sprite.x,
                                         (CGFloat)sprite.y,
                                         (CGFloat)sprite.width,
                                         (CGFloat)sprite.height)
                     fromRect:NSZeroRect
                    operation:LOKA_MAC_COMPOSITING_SOURCE_OVER
                     fraction:1.0];
        }
      }
      continue;
    }
    }
    NSRectFill(NSMakeRect((CGFloat)sprite.x, (CGFloat)sprite.y, (CGFloat)sprite.width, (CGFloat)sprite.height));
  }
}
