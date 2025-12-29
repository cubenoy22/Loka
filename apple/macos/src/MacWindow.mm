#include "MacWindow.hpp"
#include "MacApp.hpp"
#include "MacScenePlatformController.hpp"
#include <AppKit/AppKit.h>
#include "core2/scene/NodeManager.hpp"
#include "core2/scene/Scene.hpp"

namespace
{
  NSString *CreateNSStringFromUtf8(const std::string &value)
  {
    if (value.empty())
    {
      return @"";
    }
    NSString *string = [NSString stringWithUTF8String:value.c_str()];
    if (string)
    {
      return string;
    }
    NSData *data = [NSData dataWithBytes:value.data() length:value.size()];
    string = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return string ? string : @"";
  }
}

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

MacWindow::MacWindow(PlatformContext *context, declara::core::scene::Scene *initialScene, const WindowOptions &opts)
    : Window(context, initialScene, opts), window_(0), contentView_(0), delegate_(0), app_(0), closing_(false), nodeManager_(0), scenePlatformController_(0)
{
  this->visibility.deferBind(&MacWindow::VisibilityChangedThunk, this);
  this->title.deferBind(&MacWindow::TitleChangedThunk, this);
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
  window_ = 0;
  contentView_ = 0;
  delegate_ = 0;
}

void MacWindow::VisibilityChangedThunk(void *userData)
{
  MacWindow *self = static_cast<MacWindow *>(userData);
  if (!self)
  {
    return;
  }
  bool visible = self->visibility.get();
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
  [window setTitle:CreateNSStringFromUtf8(self->title.get())];
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
  [window setTitle:CreateNSStringFromUtf8(this->title.get())];

  DeclaraFlippedView *contentView = [[DeclaraFlippedView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height)];
  [window setContentView:contentView];

  DeclaraWindowDelegate *delegate = [[DeclaraWindowDelegate alloc] init];
  delegate.owner = this;
  [window setDelegate:delegate];

  window_ = (__bridge void *)window;
  contentView_ = (__bridge void *)contentView;
  delegate_ = (__bridge void *)delegate;

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
  if (this->options_.visible)
  {
    this->visibility.set(true, true);
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
  window_ = 0;
  contentView_ = 0;
  delegate_ = 0;
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
  if (nodeManager_ || !window_ || !contentView_)
  {
    return;
  }
  declara::core::scene::Scene *currentScene = this->scene();
  if (!currentScene)
  {
    return;
  }
  scenePlatformController_ = new MacScenePlatformController(contentView_);
  nodeManager_ = new declara::core::scene::StaticNodeManager();
  nodeManager_->mount(currentScene, scenePlatformController_);
}

void MacWindow::teardownScene()
{
  if (nodeManager_)
  {
    delete nodeManager_;
    nodeManager_ = 0;
  }
  if (scenePlatformController_)
  {
    delete scenePlatformController_;
    scenePlatformController_ = 0;
  }
}
