#include "MacApp.hpp"
#include "MacWindow.hpp"
#include <AppKit/AppKit.h>
#include "core/AppComponent.hpp"
#include "loka/platform/StringUTF8.hpp"

@interface LokaMenuTarget : NSObject
@property(nonatomic, assign) MacApp *owner;
@end

@implementation LokaMenuTarget
- (void)handleMenuAction:(id)sender
{
  if (self.owner)
  {
    NSInteger tag = [sender tag];
    self.owner->handleMenuCommand(static_cast<int>(tag));
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
    : App(config), nextCommandId_(1), commands_(), bindings_(), menuTarget_(0)
{
}

MacApp::~MacApp()
{
  clearMenuBindings();
  if (menuTarget_)
  {
    CFRelease(menuTarget_);
    menuTarget_ = 0;
  }
}

void MacApp::run()
{
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

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

  [NSApp activateIgnoringOtherApps:YES];
  [NSApp run];
}

void MacApp::quit()
{
  [NSApp terminate:nil];
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
  NSMenuItem *item = (__bridge NSMenuItem *)binding->menuItem;
  [item setEnabled:binding->enabledState->get()];
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
      return 0;
    }
    [menuItem setSubmenu:subMenu];
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

  if (itemDef->enabledState)
  {
    [menuItem setEnabled:itemDef->enabledState->get()];
    MacApp::MenuBinding *binding = new MacApp::MenuBinding();
    binding->menuItem = (__bridge void *)menuItem;
    binding->enabledState = itemDef->enabledState;
    itemDef->enabledState->deferBind(&MenuEnabledChangedThunk, binding);
    bindings.push_back(binding);
  }

  [menu addItem:menuItem];
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
    target = (__bridge LokaMenuTarget *)menuTarget_;
  }
  else
  {
    LokaMenuTarget *created = [[LokaMenuTarget alloc] init];
    created.owner = this;
    menuTarget_ = (__bridge_retained void *)created;
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
      continue;
    }

    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:menuTitle action:nil keyEquivalent:@""];
    [menuItem setSubmenu:subMenu];
    [mainMenu addItem:menuItem];
  }
  [NSApp setMainMenu:mainMenu];
  clearMenuDiff();
}
