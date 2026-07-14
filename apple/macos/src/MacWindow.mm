#include "MacWindow.hpp"
#include "MacApp.hpp"
#include "MacObjCCompat.hpp"
#include "MacScenePlatformController.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "app/scene/Scene.hpp"
#include "platform/StringUTF8.hpp"

@interface LokaFlippedView : NSView
{
  MacWindow *owner_;
}
- (void)setOwner:(MacWindow *)owner;
@end

@implementation LokaFlippedView
- (BOOL)isFlipped
{
  return YES;
}

- (BOOL)acceptsFirstResponder
{
  return YES;
}

- (void)setOwner:(MacWindow *)owner
{
  owner_ = owner;
}

- (void)keyDown:(NSEvent *)event
{
  if (owner_)
  {
    NSString *characters = [event charactersIgnoringModifiers];
    if (characters && [characters length] > 0)
    {
      const unichar value = [characters characterAtIndex:0];
      if (value <= 0x7f && owner_->handleKeyPress(static_cast<char>(value)))
      {
        return;
      }
    }
  }
  [super keyDown:event];
}
@end

@class LokaWindowDelegate;

@interface LokaWindowDelegate : NSObject
{
  MacWindow *owner_;
}
@property(nonatomic, assign) MacWindow *owner;
@end

@implementation LokaWindowDelegate
@synthesize owner = owner_;
- (void)windowWillClose:(NSNotification *)notification
{
  (void)notification;
  if (self.owner)
  {
    self.owner->handleWindowWillClose();
  }
}

- (void)windowDidResize:(NSNotification *)notification
{
  (void)notification;
  if (self.owner)
  {
    self.owner->handleWindowDidResize();
  }
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
  (void)notification;
  if (self.owner)
  {
    self.owner->handleWindowDidBecomeKey();
  }
}
@end

MacWindow::MacWindow(PlatformContext *context, const WindowProps &props)
    : Window(context, props),
      window_(0),
      contentView_(0),
      delegate_(0),
      app_(0),
      closing_(false),
      scenePlatformController_(0)
{
  this->visibilityState().deferBind(&MacWindow::VisibilityChangedThunk, this);
  this->titleState().deferBind(&MacWindow::TitleChangedThunk, this);
  this->frameState().deferBind(&MacWindow::FrameChangedThunk, this);
}

MacWindow::~MacWindow()
{
  this->visibilityState().deferUnbind(&MacWindow::VisibilityChangedThunk, this);
  this->titleState().deferUnbind(&MacWindow::TitleChangedThunk, this);
  this->frameState().deferUnbind(&MacWindow::FrameChangedThunk, this);
  teardownScene();
  NSWindow *window = (NSWindow *)window_;
  if (window)
  {
    [window setDelegate:nil];
    [window close];
  }
  if (contentView_)
  {
    [(id)contentView_ release];
    contentView_ = 0;
  }
  if (window_)
  {
    [(id)window_ release];
    window_ = 0;
  }
  if (delegate_)
  {
    [(id)delegate_ release];
    delegate_ = 0;
  }
}

void MacWindow::setApp(App *app)
{
  app_ = app;
  if (app_)
  {
    app_->setActiveWindow(this);
  }
}

void MacWindow::VisibilityChangedThunk(void *userData)
{
  MacWindow *self = static_cast<MacWindow *>(userData);
  if (!self)
  {
    return;
  }
  bool visible = self->visibilityState().get();
  if (visible)
  {
    if (!self->window_)
    {
      self->createNativeWindow();
    }
    if (self->window_)
    {
      self->onShow();
    }
  }
  else
  {
    if (self->window_)
    {
      self->onHide();
      self->destroyNativeWindow();
    }
  }
}

void MacWindow::TitleChangedThunk(void *userData)
{
  MacWindow *self = static_cast<MacWindow *>(userData);
  if (!self || !self->window_)
  {
    return;
  }
  NSWindow *window = (NSWindow *)self->window_;
  std::string utf8;
  if (loka::platform::CollectUtf8(self->titleState().get(), utf8))
  {
    [window setTitle:loka::macos::CreateNSStringFromUtf8(utf8)];
  }
  else
  {
    [window setTitle:@""];
  }
}

namespace
{
  static CGFloat VisibleTopForScreen(NSScreen *screen)
  {
    if (!screen)
    {
      return 0.0;
    }
    NSRect screenFrame = [screen frame];
    NSRect visibleFrame = [screen visibleFrame];
    CGFloat top = visibleFrame.origin.y + visibleFrame.size.height;
    if (visibleFrame.size.height >= screenFrame.size.height - 1.0)
    {
      CGFloat menuHeight = [[NSStatusBar systemStatusBar] thickness];
      top = screenFrame.origin.y + screenFrame.size.height - menuHeight;
    }
    return top;
  }

  static NSRect
  FrameRectForContent(CGFloat x, CGFloat y, CGFloat width, CGFloat height, NSUInteger style, NSScreen *screen)
  {
    NSRect contentRect = NSMakeRect(0.0, 0.0, width, height);
    NSRect frameRect = [NSWindow frameRectForContentRect:contentRect styleMask:style];
    CGFloat top = VisibleTopForScreen(screen);
    frameRect.origin.x = x;
    frameRect.origin.y = top - y - frameRect.size.height;
    return frameRect;
  }
} // namespace

void MacWindow::FrameChangedThunk(void *userData)
{
  MacWindow *self = static_cast<MacWindow *>(userData);
  if (!self || !self->window_)
  {
    return;
  }
  NSWindow *window = (NSWindow *)self->window_;
  loka::core::Frame frame = self->frameState().get();
  if (!frame.hasSize())
  {
    return;
  }
  NSRect currentFrame = [window frame];
  NSRect currentContent = [window contentRectForFrameRect:currentFrame];
  CGFloat x = frame.x >= 0 ? frame.x : currentContent.origin.x;
  CGFloat y = frame.y >= 0 ? frame.y : currentContent.origin.y;
  CGFloat width = frame.width > 0 ? frame.width : currentContent.size.width;
  CGFloat height = frame.height > 0 ? frame.height : currentContent.size.height;
  NSScreen *screen = [window screen];
  if (!screen)
  {
    screen = [NSScreen mainScreen];
  }
  NSRect nextFrame = FrameRectForContent(x, y, width, height, [window styleMask], screen);
  if (frame.x < 0)
  {
    nextFrame.origin.x = currentFrame.origin.x;
  }
  if (frame.y < 0)
  {
    nextFrame.origin.y = currentFrame.origin.y;
  }
  [window setFrame:nextFrame display:YES];
}

void MacWindow::createNativeWindow()
{
  if (window_)
  {
    return;
  }
  closing_ = false;
  CGFloat x = this->hasPosition() ? this->positionX() : 50;
  CGFloat y = this->hasPosition() ? this->positionY() : 50;
  CGFloat width = this->hasSize() ? this->width() : 300;
  CGFloat height = this->hasSize() ? this->height() : 300;
#if defined(NSWindowStyleMaskTitled)
  NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                     | NSWindowStyleMaskMiniaturizable;
#else
  NSUInteger style = NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask;
#endif
  NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0.0, 0.0, width, height)
                                                 styleMask:style
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
  NSScreen *screen = [NSScreen mainScreen];
  NSRect frame = FrameRectForContent(x, y, width, height, style, screen);
  [window setFrame:frame display:NO];
  std::string utf8;
  if (loka::platform::CollectUtf8(this->titleState().get(), utf8))
  {
    [window setTitle:loka::macos::CreateNSStringFromUtf8(utf8)];
  }
  else
  {
    [window setTitle:@""];
  }

  LokaFlippedView *contentView = [[LokaFlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
  [window setContentView:contentView];
  [contentView setOwner:this];
  [window makeFirstResponder:contentView];

  LokaWindowDelegate *delegate = [[LokaWindowDelegate alloc] init];
  delegate.owner = this;
  [window setDelegate:delegate];

  window_ = (void *)window;
  contentView_ = (void *)contentView;
  delegate_ = (void *)delegate;

  [window makeKeyAndOrderFront:nil];
  this->onCreate();
  this->mountScene();
}

bool MacWindow::handleKeyPress(char key)
{
  return app_ ? app_->handleKeyPress(key) : false;
}

void MacWindow::destroyNativeWindow()
{
  NSWindow *window = (NSWindow *)window_;
  if (!window)
  {
    return;
  }
  [window close];
}

void MacWindow::onCreate()
{
  Window::onCreate();
  if (this->visibilityState().get())
  {
    this->visibilityState().set(true, true);
  }
}

void MacWindow::onShow()
{
  NSWindow *window = (NSWindow *)window_;
  if (window)
  {
    [window makeKeyAndOrderFront:nil];
  }
}

void MacWindow::onHide() {}

void MacWindow::synchronizeScenePlatform()
{
  if (scenePlatformController_)
  {
    scenePlatformController_->synchronize();
  }
}

bool MacWindow::hasPendingScenePlatformSync() const
{
  return scenePlatformController_ ? scenePlatformController_->hasPendingSync() : false;
}

void MacWindow::handleWindowWillClose()
{
  if (closing_)
  {
    return;
  }
  closing_ = true;
  App *app = app_;
  NSWindow *window = (NSWindow *)window_;
  id contentView = (id)contentView_;
  id delegate = (id)delegate_;
  if (window)
  {
    [window setDelegate:nil];
  }
  if (delegate)
  {
    [(LokaWindowDelegate *)delegate setOwner:0];
  }
  if (contentView)
  {
    [(LokaFlippedView *)contentView setOwner:0];
  }
  teardownScene();
  this->onDestroy();
  window_ = 0;
  contentView_ = 0;
  delegate_ = 0;
  app_ = 0;
  // On 10.6 the window-close shutdown path can still crash inside NSWindow
  // dealloc/free while AppKit is unwinding this callback. Keep the native
  // objects detached from our C++ side here and let process teardown reclaim
  // them instead of trying to release them during close.
  (void)delegate;
  (void)contentView;
  (void)window;
  if (app)
  {
    app->requestWindowClose(static_cast<Window *>(this));
  }
}

void MacWindow::handleWindowDidResize()
{
  if (!scenePlatformController_ || !contentView_)
  {
    return;
  }
  NSView *view = (NSView *)contentView_;
  NSRect bounds = [view bounds];
  scenePlatformController_->relayout(static_cast<int>(bounds.size.width), static_cast<int>(bounds.size.height));
}

void MacWindow::handleWindowDidBecomeKey()
{
  if (app_)
  {
    app_->setActiveWindow(this);
  }
}

void MacWindow::mountScene()
{
  if (scenePlatformController_ || !window_ || !contentView_)
  {
    return;
  }
  loka::app::scene::Scene *currentScene = this->scene();
  if (!currentScene)
  {
    return;
  }
  scenePlatformController_ = new MacScenePlatformController(contentView_);
  currentScene->mount(scenePlatformController_);
}

void MacWindow::teardownScene()
{
  loka::app::scene::Scene *currentScene = this->scene();
  if (currentScene)
  {
    currentScene->unmount();
  }
  if (scenePlatformController_)
  {
    delete scenePlatformController_;
    scenePlatformController_ = 0;
  }
}
