#include "MacCellContext.hpp"
#include "../MacScenePlatformController.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/nodes/controls/Cell.hpp"
#include "core/State.hpp"
#include "platform/StringUTF8.hpp"

class MacCellContext;

namespace
{
  const int kDefaultCellHeight = 20;
  const int kVerticalSpacing = 12;

  class MacCellNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::CellNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::CellNode *cell = node ? node->asCellNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!cell || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureCellContext(cell,
                                                     state.x,
                                                     state.y,
                                                     state.width,
                                                     state.height);
    }
  };

  MacCellNodeHandler gMacCellNodeHandler;
}

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
    : node_(node), view_(0), textState_(0), textStateBound_(false)
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

void MacCellContext::onNodeAttached()
{
  LokaCellView *view = (LokaCellView *)view_;
  if (view)
  {
    [view setHidden:NO];
  }
}

void MacCellContext::onNodeDetached()
{
  LokaCellView *view = (LokaCellView *)view_;
  if (view)
  {
    [view setHidden:YES];
  }
}

short MacCellContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  const short requestedHeight = state.height;
  const int cellHeight = requestedHeight > 0 ? requestedHeight : kDefaultCellHeight;
  this->relayout(state.x, state.y, state.width, cellHeight);
  state.height = static_cast<short>(cellHeight);
  short result = static_cast<short>(state.y + cellHeight);
  if (requestedHeight <= 0)
  {
    result = static_cast<short>(result + kVerticalSpacing);
  }
  return result;
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
  textStateBound_ = false;
  if (textState_)
  {
    if (!node_->props.ownsText_)
    {
      textState_->deferBind(&MacCellContext::TextChangedThunk, this);
      textStateBound_ = true;
    }
    applyText();
  }
}

void MacCellContext::unbindText()
{
  if (textState_)
  {
    if (textStateBound_)
    {
      textState_->deferUnbind(&MacCellContext::TextChangedThunk, this);
    }
    textState_ = 0;
    textStateBound_ = false;
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

void RegisterMacCellNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gMacCellNodeHandler);
}
