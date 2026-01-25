#include "MacWindow.hpp"
#include "MacApp.hpp"
#include "MacScenePlatformController.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "core2/scene/Scene.hpp"
#include "loka/platform/StringUTF8.hpp"

@interface LokaFlippedView : NSView
@end

@implementation LokaFlippedView
- (BOOL)isFlipped
{
  return YES;
}
@end

@class LokaWindowDelegate;

@interface LokaWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) MacWindow *owner;
@end

@implementation LokaWindowDelegate
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
    : Window(context, props), window_(0), contentView_(0), delegate_(0), app_(0), closing_(false), scenePlatformController_(0)
{
  this->visibilityState().deferBind(&MacWindow::VisibilityChangedThunk, this);
  this->titleState().deferBind(&MacWindow::TitleChangedThunk, this);
  this->frameState().deferBind(&MacWindow::FrameChangedThunk, this);
}

MacWindow::~MacWindow()
{
  this->visibilityState().deferUnbind(&MacWindow::VisibilityChangedThunk, this);
  this->titleState().deferUnbind(&MacWindow::TitleChangedThunk, this);
  teardownScene();
  NSWindow *window = (__bridge NSWindow *)window_;
  if (window)
  {
    [window setDelegate:nil];
    [window close];
  }
  if (delegate_)
  {
    CFRelease(delegate_);
    delegate_ = 0;
  }
  window_ = 0;
  contentView_ = 0;
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
  NSWindow *window = (__bridge NSWindow *)self->window_;
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

  static NSRect FrameRectForContent(CGFloat x, CGFloat y, CGFloat width, CGFloat height, NSUInteger style, NSScreen *screen)
  {
    NSRect contentRect = NSMakeRect(0.0, 0.0, width, height);
    NSRect frameRect = [NSWindow frameRectForContentRect:contentRect styleMask:style];
    CGFloat top = VisibleTopForScreen(screen);
    frameRect.origin.x = x;
    frameRect.origin.y = top - y - frameRect.size.height;
    return frameRect;
  }
}

void MacWindow::FrameChangedThunk(void *userData)
{
  MacWindow *self = static_cast<MacWindow *>(userData);
  if (!self || !self->window_)
  {
    return;
  }
  NSWindow *window = (__bridge NSWindow *)self->window_;
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
  NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                     NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
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

  LokaWindowDelegate *delegate = [[LokaWindowDelegate alloc] init];
  delegate.owner = this;
  [window setDelegate:delegate];

  window_ = (__bridge void *)window;
  contentView_ = (__bridge void *)contentView;
  delegate_ = (__bridge_retained void *)delegate;

  [window makeKeyAndOrderFront:nil];
  this->onCreate();
  this->mountScene();
}

void MacWindow::destroyNativeWindow()
{
  NSWindow *window = (__bridge NSWindow *)window_;
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
  NSWindow *window = (__bridge NSWindow *)window_;
  if (window)
  {
    [window makeKeyAndOrderFront:nil];
  }
}

void MacWindow::onHide()
{
}

void MacWindow::handleWindowWillClose()
{
  if (closing_)
  {
    return;
  }
  closing_ = true;
  teardownScene();
  this->onDestroy();
  if (delegate_)
  {
    CFRelease(delegate_);
    delegate_ = 0;
  }
  window_ = 0;
  contentView_ = 0;
  if (app_)
  {
    app_->windowClosed(static_cast<Window *>(this));
  }
}

void MacWindow::handleWindowDidResize()
{
  if (!scenePlatformController_ || !contentView_)
  {
    return;
  }
  NSView *view = (__bridge NSView *)contentView_;
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
  loka::core::scene::Scene *currentScene = this->scene();
  if (!currentScene)
  {
    return;
  }
  scenePlatformController_ = new MacScenePlatformController(contentView_);
  currentScene->mount(scenePlatformController_);
}

void MacWindow::teardownScene()
{
  loka::core::scene::Scene *currentScene = this->scene();
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
