#include "MacApp.hpp"
#include "MacWindow.hpp"
#include "MacObjCCompat.hpp"
#include <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include "app/AppComponent.hpp"
#include "loka/platform/StringUTF8.hpp"

@interface LokaMenuTarget : NSObject
{
  MacApp *owner_;
}
@property(nonatomic, assign) MacApp *owner;
@end

@implementation LokaMenuTarget
@synthesize owner = owner_;
- (void)handleMenuAction:(id)sender
{
  if (self.owner)
  {
    NSInteger tag = [sender tag];
    self.owner->handleMenuCommand(static_cast<int>(tag));
  }
}
@end

@interface LokaFlushTarget : NSObject
{
  MacApp *owner_;
}
@property(nonatomic, assign) MacApp *owner;
@end

@implementation LokaFlushTarget
@synthesize owner = owner_;
- (void)handleFlush:(id)sender
{
  (void)sender;
  if (self.owner)
  {
    self.owner->flushInvalidationsTick();
  }
}
@end

namespace
{
  static NSString *MenuTitleFromString(const loka::core::String &title, const char *fallback)
  {
    std::string utf8;
    if (loka::platform::CollectUtf8(title, utf8) && !utf8.empty())
    {
      return [NSString stringWithUTF8String:utf8.c_str()];
    }
    return [NSString stringWithUTF8String:fallback];
  }
}

MacApp::MacApp(AppConfigurable *config)
    : App(config), nextCommandId_(1), commands_(), bindings_(), menuTarget_(0), flushTarget_(0), flushTimer_(0)
{
}

MacApp::~MacApp()
{
  clearMenuBindings();
  if (menuTarget_)
  {
    [(id)menuTarget_ release];
    menuTarget_ = 0;
  }
  stopInvalidationFlushTimer();
}

void MacApp::run()
{
  ProcessSerialNumber psn = {0, kCurrentProcess};
  TransformProcessType(&psn, kProcessTransformToForegroundApplication);
  SetFrontProcess(&psn);

  [NSApplication sharedApplication];
  if ([NSApp respondsToSelector:@selector(setActivationPolicy:)])
  {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  }

  App::run();

  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      Window *w = (*it)->asWindow();
      MacWindow *macWin = w ? w->asMacWindow() : 0;
      if (macWin)
      {
        macWin->setApp(this);
      }
    }
  }

  startInvalidationFlushTimer();
  [NSApp activateIgnoringOtherApps:YES];
  [NSApp run];
  stopInvalidationFlushTimer();
}

void MacApp::quit()
{
  stopInvalidationFlushTimer();
  [NSApp terminate:nil];
}

void MacApp::flushInvalidationsTick()
{
  this->flushWindowInvalidations();
}

void MacApp::startInvalidationFlushTimer()
{
  if (flushTimer_)
  {
    return;
  }
  LokaFlushTarget *target = [[LokaFlushTarget alloc] init];
  [target setOwner:this];
  NSTimer *timer = [NSTimer timerWithTimeInterval:0.0
                                           target:target
                                         selector:@selector(handleFlush:)
                                         userInfo:nil
                                          repeats:YES];
  if (!timer)
  {
    [target release];
    return;
  }
  [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
  [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
  flushTarget_ = target;
  flushTimer_ = timer;
}

void MacApp::stopInvalidationFlushTimer()
{
  if (flushTimer_)
  {
    [(NSTimer *)flushTimer_ invalidate];
    flushTimer_ = 0;
  }
  if (flushTarget_)
  {
    [(id)flushTarget_ release];
    flushTarget_ = 0;
  }
}

void MacApp::handleMenuCommand(int commandId)
{
  for (size_t i = 0; i < commands_.size(); ++i)
  {
    if (commands_[i].commandId != commandId)
      continue;
    switch (commands_[i].action)
    {
    case loka::app::MENU_ACTION_ABOUT_APP:
      [NSApp orderFrontStandardAboutPanel:nil];
      return;
    case loka::app::MENU_ACTION_SHOW_COLOR_PICKER:
      [NSApp orderFrontColorPanel:nil];
      return;
    case loka::app::MENU_ACTION_QUIT_APP:
      quit();
      return;
    case loka::app::MENU_ACTION_REBUILD_MENU:
      break;
    case loka::app::MENU_ACTION_NONE:
    default:
      break;
    }
    if (commands_[i].emitter)
    {
      commands_[i].emitter->emit();
    }
    if (commands_[i].action == loka::app::MENU_ACTION_REBUILD_MENU)
    {
      invalidateMenu();
    }
    return;
  }
}

static void MenuEnabledChangedThunk(void *userData)
{
  MacApp::MenuBinding *binding = static_cast<MacApp::MenuBinding *>(userData);
  if (!binding || !binding->menuItem || !binding->enabledState)
    return;
  NSMenuItem *item = (NSMenuItem *)binding->menuItem;
  bool enabled = binding->enabledState->get();
  if (binding->invertEnabled)
  {
    enabled = !enabled;
  }
  [item setEnabled:enabled ? YES : NO];
}

void MacApp::clearMenuBindings()
{
  for (size_t i = 0; i < bindings_.size(); ++i)
  {
    MenuBinding *binding = bindings_[i];
    if (binding && binding->enabledState)
    {
      binding->enabledState->deferUnbind(&MenuEnabledChangedThunk, binding);
    }
    delete binding;
  }
  bindings_.clear();
  commands_.clear();
  nextCommandId_ = 1;
}

static NSString *MenuShortcutForAction(const loka::app::MenuItemDefinition *itemDef)
{
  if (!itemDef)
    return @"";
  if (itemDef->hasShortcut && itemDef->shortcutKey)
  {
    char buf[2] = {itemDef->shortcutKey, 0};
    return [NSString stringWithUTF8String:buf];
  }
  switch (itemDef->action)
  {
  case loka::app::MENU_ACTION_QUIT_APP:
    return @"q";
  default:
    return @"";
  }
}

static std::size_t BuildMenuItems(NSMenu *menu,
                                  const loka::app::MenuItemDefinition *itemsHead,
                                  LokaMenuTarget *target,
                                  std::vector<MacApp::MenuCommand> &commands,
                                  std::vector<MacApp::MenuBinding *> &bindings,
                                  int &nextCommandId,
                                  bool allowQuit);

static std::size_t BuildMenuItem(NSMenu *menu,
                                 const loka::app::MenuItemDefinition *itemDef,
                                 LokaMenuTarget *target,
                                 std::vector<MacApp::MenuCommand> &commands,
                                 std::vector<MacApp::MenuBinding *> &bindings,
                                 int &nextCommandId,
                                 bool allowQuit)
{
  if (!itemDef)
    return 0;
  if (!itemDef->isVisibleInitial())
    return 0;
  if (!allowQuit && itemDef->action == loka::app::MENU_ACTION_QUIT_APP)
    return 0;
  if (itemDef->isSeparator)
  {
    [menu addItem:[NSMenuItem separatorItem]];
    return 1;
  }

  NSString *title = MenuTitleFromString(itemDef->title, "Menu Item");
  NSString *shortcut = MenuShortcutForAction(itemDef);
  NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:shortcut];

  if (itemDef->hasChildren())
  {
    NSMenu *subMenu = [[NSMenu alloc] initWithTitle:title];
    if (BuildMenuItems(subMenu, itemDef->childrenHead(), target, commands, bindings, nextCommandId, allowQuit) == 0)
    {
      [subMenu release];
      [menuItem release];
      return 0;
    }
    [menuItem setSubmenu:subMenu];
    [subMenu release];
  }
  else
  {
    [menuItem setTarget:target];
    [menuItem setAction:@selector(handleMenuAction:)];
    int commandId = nextCommandId++;
    [menuItem setTag:commandId];
    MacApp::MenuCommand command;
    command.commandId = commandId;
    command.action = itemDef->action;
    command.emitter = itemDef->onClickState;
    commands.push_back(command);
  }

  if (!itemDef->isEnabledInitial())
  {
    [menuItem setEnabled:NO];
  }

  loka::core::State<bool> *enabledBindingState = itemDef->enabledBindingState();
  if (enabledBindingState)
  {
    [menuItem setEnabled:itemDef->isEnabledInitial() ? YES : NO];
    MacApp::MenuBinding *binding = new MacApp::MenuBinding();
    binding->menuItem = (void *)menuItem;
    binding->enabledState = enabledBindingState;
    binding->invertEnabled = itemDef->enabledBindingInvert();
    enabledBindingState->deferBind(&MenuEnabledChangedThunk, binding);
    bindings.push_back(binding);
  }

  [menu addItem:menuItem];
  [menuItem release];
  return 1;
}

static std::size_t BuildMenuItems(NSMenu *menu,
                                  const loka::app::MenuItemDefinition *itemsHead,
                                  LokaMenuTarget *target,
                                  std::vector<MacApp::MenuCommand> &commands,
                                  std::vector<MacApp::MenuBinding *> &bindings,
                                  int &nextCommandId,
                                  bool allowQuit)
{
  std::size_t added = 0;
  const loka::app::MenuItemDefinition *itemDef = itemsHead;
  while (itemDef)
  {
    added += BuildMenuItem(menu, itemDef, target, commands, bindings, nextCommandId, allowQuit);
    itemDef = itemDef->nextInComposition;
  }
  return added;
}

void MacApp::applyMenuBar(Window *activeWindow)
{
  clearMenuBindings();
  const loka::app::MenuBarDefinition *menuBar = resolveMenuBar(activeWindow);
  if (!menuBar)
  {
    return;
  }

  bool hasAppMenu = false;
  loka::dsl::CompositionCursor<loka::app::MenuDefinition> appMenuScan(menuBar->menusHead(), menuBar->menusCount());
  for (loka::app::MenuDefinition *menuDef = appMenuScan.next(); menuDef; menuDef = appMenuScan.next())
  {
    if (menuDef->isAppMenu)
    {
      hasAppMenu = true;
      break;
    }
  }

  LokaMenuTarget *target = nil;
  if (menuTarget_)
  {
    target = (LokaMenuTarget *)menuTarget_;
  }
  else
  {
    LokaMenuTarget *created = [[LokaMenuTarget alloc] init];
    created.owner = this;
    menuTarget_ = (void *)created;
    target = created;
  }

  NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@""];
  loka::dsl::CompositionCursor<loka::app::MenuDefinition> it(menuBar->menusHead(), menuBar->menusCount());
  for (loka::app::MenuDefinition *menuDef = it.next(); menuDef; menuDef = it.next())
  {
    const char *fallback = menuDef->isAppMenu ? "Loka" : "Menu";
    NSString *menuTitle = MenuTitleFromString(menuDef->title, fallback);
    NSMenu *subMenu = [[NSMenu alloc] initWithTitle:menuTitle];
    bool allowQuit = !menuDef->isAppMenu;
    if (menuDef->isAppMenu)
    {
      allowQuit = true;
    }
    else if (hasAppMenu)
    {
      allowQuit = false;
    }
    if (BuildMenuItems(subMenu, menuDef->itemsHead(), target, commands_, bindings_, nextCommandId_, allowQuit) == 0)
    {
      [subMenu release];
      continue;
    }

    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:menuTitle action:nil keyEquivalent:@""];
    [menuItem setSubmenu:subMenu];
    [mainMenu addItem:menuItem];
    [menuItem release];
    [subMenu release];
  }
  [NSApp setMainMenu:mainMenu];
  [mainMenu release];
  clearMenuDiff();
}
