#include "MacWindow.hpp"
#include "MacApp.hpp"
#include "MacScenePlatformController.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "core2/scene/Scene.hpp"

@interface DeclaraFlippedView : NSView
@end

@implementation DeclaraFlippedView
- (BOOL)isFlipped
{
  return YES;
}
@end

@class DeclaraWindowDelegate;

@interface DeclaraWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) MacWindow *owner;
@end

@implementation DeclaraWindowDelegate
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
@end

MacWindow::MacWindow(PlatformContext *context, const WindowProps &props)
    : Window(context, props), window_(0), contentView_(0), delegate_(0), app_(0), closing_(false), scenePlatformController_(0)
{
  this->visibilityState().deferBind(&MacWindow::VisibilityChangedThunk, this);
  this->titleState().deferBind(&MacWindow::TitleChangedThunk, this);
}

MacWindow::~MacWindow()
{
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
  [window setTitle:declara::macos::CreateNSStringFromUtf8(self->titleState().get())];
}

void MacWindow::createNativeWindow()
{
  if (window_)
  {
    return;
  }
  closing_ = false;
  NSRect frame = NSMakeRect(100.0, 100.0, 640.0, 360.0);
  NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                     NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
  NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                 styleMask:style
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
  [window setTitle:declara::macos::CreateNSStringFromUtf8(this->titleState().get())];

  DeclaraFlippedView *contentView = [[DeclaraFlippedView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height)];
  [window setContentView:contentView];

  DeclaraWindowDelegate *delegate = [[DeclaraWindowDelegate alloc] init];
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

void MacWindow::mountScene()
{
  if (scenePlatformController_ || !window_ || !contentView_)
  {
    return;
  }
  declara::core::scene::Scene *currentScene = this->scene();
  if (!currentScene)
  {
    return;
  }
  scenePlatformController_ = new MacScenePlatformController(contentView_);
  currentScene->mount(scenePlatformController_);
}

void MacWindow::teardownScene()
{
  declara::core::scene::Scene *currentScene = this->scene();
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
