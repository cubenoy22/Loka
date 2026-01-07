#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"

#include <Dialogs.h>
#include <Events.h>
#include <Fonts.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <Sound.h>
#include <TextEdit.h>
#include <Windows.h>
#include "loka/platform/StringUTF8.hpp"

ToolboxApp::ToolboxApp(AppConfigurable *config)
    : App(config), nextMenuId_(128), commands_(), bindings_(), running_(false)
{
}
ToolboxApp::~ToolboxApp()
{
  clearMenuBindings();
}

void ToolboxApp::run()
{
  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(0);
  InitCursor();

  App::run();
  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    ToolboxWindow *firstWindow = 0;
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      ToolboxWindow *toolboxWindow = dynamic_cast<ToolboxWindow *>(*it);
      if (toolboxWindow)
      {
        if (!firstWindow)
        {
          firstWindow = toolboxWindow;
        }
        toolboxWindow->setApp(this);
      }
    }
    if (!activeWindow() && firstWindow)
    {
      setActiveWindow(firstWindow);
    }
  }
  running_ = true;
  while (running_)
  {
    EventRecord event;
    WaitNextEvent(everyEvent, &event, 1, 0);
    if (event.what == updateEvt)
    {
      WindowPtr target = reinterpret_cast<WindowPtr>(event.message);
      BeginUpdate(target);
      if (group_)
      {
        const std::vector<AppComponent *> &comps = group_->getComponents();
        for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
        {
          ToolboxWindow *toolboxWindow = dynamic_cast<ToolboxWindow *>(*it);
          if (toolboxWindow && toolboxWindow->window() == target)
          {
            toolboxWindow->draw();
            break;
          }
        }
      }
      EndUpdate(target);
    }
    else if (event.what == mouseDown)
    {
      WindowPtr target = 0;
      short part = FindWindow(event.where, &target);
      if (part == inMenuBar)
      {
        long choice = MenuSelect(event.where);
        if (choice != 0)
        {
          short menuId = static_cast<short>(choice >> 16);
          short item = static_cast<short>(choice & 0xFFFF);
          handleMenuCommand(menuId, item);
          HiliteMenu(0);
        }
        continue;
      }
      if (part == inGoAway && target)
      {
        if (TrackGoAway(target, event.where))
        {
          ToolboxWindow *closing = 0;
          if (group_)
          {
            const std::vector<AppComponent *> &comps = group_->getComponents();
            for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
            {
              ToolboxWindow *toolboxWindow = dynamic_cast<ToolboxWindow *>(*it);
              if (toolboxWindow && toolboxWindow->window() == target)
              {
                closing = toolboxWindow;
                break;
              }
            }
          }
          if (closing)
          {
            closing->invalidateWindow();
            windowClosed(closing);
          }
        }
      }
      else if (part == inDrag && target)
      {
        Rect bounds = qd.screenBits.bounds;
        DragWindow(target, event.where, &bounds);
      }
    }
    else if (event.what == activateEvt)
    {
      WindowPtr target = reinterpret_cast<WindowPtr>(event.message);
      if (group_)
      {
        const std::vector<AppComponent *> &comps = group_->getComponents();
        for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
        {
          ToolboxWindow *toolboxWindow = dynamic_cast<ToolboxWindow *>(*it);
          if (toolboxWindow && toolboxWindow->window() == target)
          {
            setActiveWindow(toolboxWindow);
            break;
          }
        }
      }
    }
    else if (event.what == keyDown || event.what == autoKey)
    {
      long choice = MenuKey(static_cast<char>(event.message & charCodeMask));
      if (choice != 0)
      {
        short menuId = static_cast<short>(choice >> 16);
        short item = static_cast<short>(choice & 0xFFFF);
        handleMenuCommand(menuId, item);
        HiliteMenu(0);
      }
    }
  }
}

void ToolboxApp::quit()
{
  running_ = false;
}

void ToolboxApp::windowClosed(Window *window)
{
  App::windowClosed(window);
}

static void CopyToPascalString(const loka::core::String &value, Str255 out)
{
  std::string utf8;
  if (!loka::platform::CollectUtf8(value, utf8))
  {
    out[0] = 0;
    return;
  }
  std::size_t length = utf8.size();
  if (length > 255)
    length = 255;
  out[0] = static_cast<unsigned char>(length);
  for (std::size_t i = 0; i < length; ++i)
    out[1 + i] = static_cast<unsigned char>(utf8[i]);
}

void ToolboxApp::MenuEnabledChangedThunk(void *userData)
{
  ToolboxApp::MenuBinding *binding = static_cast<ToolboxApp::MenuBinding *>(userData);
  if (!binding || !binding->menu || !binding->enabledState)
    return;
  if (binding->enabledState->get())
  {
    EnableItem(binding->menu, binding->itemIndex);
  }
  else
  {
    DisableItem(binding->menu, binding->itemIndex);
  }
  DrawMenuBar();
}

void ToolboxApp::clearMenuBindings()
{
  for (size_t i = 0; i < bindings_.size(); ++i)
  {
    MenuBinding *binding = bindings_[i];
    if (binding && binding->enabledState)
    {
      binding->enabledState->deferUnbind(&ToolboxApp::MenuEnabledChangedThunk, binding);
    }
    delete binding;
  }
  bindings_.clear();
  commands_.clear();
}

void ToolboxApp::clearMenuBindingsFor(MenuHandle menuHandle, short menuId)
{
  for (size_t i = 0; i < bindings_.size();)
  {
    MenuBinding *binding = bindings_[i];
    if (binding && binding->menu == menuHandle)
    {
      if (binding->enabledState)
      {
        binding->enabledState->deferUnbind(&ToolboxApp::MenuEnabledChangedThunk, binding);
      }
      delete binding;
      bindings_.erase(bindings_.begin() + i);
      continue;
    }
    ++i;
  }
  for (size_t i = 0; i < commands_.size();)
  {
    if (commands_[i].menuId == menuId)
    {
      commands_.erase(commands_.begin() + i);
      continue;
    }
    ++i;
  }
}

void ToolboxApp::disposeMenuEntries()
{
  for (size_t i = 0; i < menuEntries_.size(); ++i)
  {
    if (menuEntries_[i].menu)
    {
      DisposeMenu(menuEntries_[i].menu);
    }
  }
  menuEntries_.clear();
}

void ToolboxApp::resetMenuState()
{
  clearMenuBindings();
  disposeMenuEntries();
  nextMenuId_ = 128;
}

static void BuildMenuItems(MenuHandle menu,
                           const declara::app::MenuItemDefinition *itemsHead,
                           short menuId,
                           std::vector<ToolboxApp::MenuCommand> &commands,
                           std::vector<ToolboxApp::MenuBinding *> &bindings)
{
  const declara::app::MenuItemDefinition *itemDef = itemsHead;
  while (itemDef)
  {
    if (!itemDef)
    {
      itemDef = itemDef->nextInComposition;
      continue;
    }
    if (itemDef->isSeparator)
    {
      AppendMenu(menu, "\p-");
      itemDef = itemDef->nextInComposition;
      continue;
    }
    if (itemDef->action == declara::app::MENU_ACTION_SHOW_COLOR_PICKER)
    {
      itemDef = itemDef->nextInComposition;
      continue;
    }
    Str255 title;
    CopyToPascalString(itemDef->title, title);
    AppendMenu(menu, title);
    short itemIndex = CountMenuItems(menu);
    ToolboxApp::MenuCommand command;
    command.menuId = menuId;
    command.itemIndex = itemIndex;
    command.action = itemDef->action;
    command.emitter = itemDef->onClickState;
    commands.push_back(command);
    if (itemDef->enabledState)
    {
      if (!itemDef->enabledState->get())
      {
        DisableItem(menu, itemIndex);
      }
      ToolboxApp::MenuBinding *binding = new ToolboxApp::MenuBinding();
      binding->menu = menu;
      binding->itemIndex = itemIndex;
      binding->enabledState = itemDef->enabledState;
      itemDef->enabledState->deferBind(&ToolboxApp::MenuEnabledChangedThunk, binding);
      bindings.push_back(binding);
    }
    itemDef = itemDef->nextInComposition;
  }
}

void ToolboxApp::applyMenuBar(Window *activeWindow)
{
  const declara::app::MenuBarDefinition *menuBar = resolveMenuBar(activeWindow);
  if (!menuBar)
  {
    resetMenuState();
    ClearMenuBar();
    InitMenus();
    DrawMenuBar();
    clearMenuDiff();
    return;
  }

  bool forceFullRebuild = (activeWindow && activeWindow->menuBar());
  const declara::app::MenuCompositionDiff &diff = menuDiff();
  if (!diff.valid && !forceFullRebuild)
  {
    return;
  }
  bool canPartial = diff.valid && !diff.fullRebuild && !forceFullRebuild;
  if (canPartial && menuEntries_.size() != menuBar->menus.size())
  {
    canPartial = false;
  }
  if (!canPartial)
  {
    resetMenuState();
    ClearMenuBar();
    InitMenus();
  }

  bool hasAppMenu = false;
  for (size_t i = 0; i < menuBar->menus.size(); ++i)
  {
    const declara::app::MenuDefinition *menuDef = menuBar->menus[i];
    if (menuDef && menuDef->isAppMenu)
    {
      hasAppMenu = true;
      break;
    }
  }

  MenuHandle appMenuHandle = 0;
  if (hasAppMenu && !canPartial)
  {
    Str255 title;
    title[0] = 1;
    title[1] = 0x14;
    MenuHandle menu = NewMenu(128, title);
    std::vector<declara::app::MenuItemDefinition *> aboutItems;
    for (size_t i = 0; i < menuBar->menus.size(); ++i)
    {
      const declara::app::MenuDefinition *menuDef = menuBar->menus[i];
      if (!menuDef || !menuDef->isAppMenu)
        continue;
      const declara::app::MenuItemDefinition *itemDef = menuDef->itemsHead();
      while (itemDef)
      {
        if (itemDef && itemDef->action == declara::app::MENU_ACTION_ABOUT_APP)
        {
          aboutItems.push_back(itemDef);
        }
        itemDef = itemDef->nextInComposition;
      }
    }
    if (!aboutItems.empty())
    {
      AppendResMenu(menu, 'DRVR');
      declara::app::MenuItemDefinition *itemDef = aboutItems[0];
      Str255 aboutTitle;
      CopyToPascalString(itemDef->title, aboutTitle);
      InsertMenuItem(menu, aboutTitle, 0);
      short aboutIndex = 1;
      // Separator not needed; desk accessories already have one.
      ToolboxApp::MenuCommand command;
      command.menuId = 128;
      command.itemIndex = aboutIndex;
      command.action = itemDef->action;
      command.emitter = itemDef->onClickState;
      commands_.push_back(command);
      if (itemDef->enabledState)
      {
        if (!itemDef->enabledState->get())
        {
          DisableItem(menu, aboutIndex);
        }
        ToolboxApp::MenuBinding *binding = new ToolboxApp::MenuBinding();
        binding->menu = menu;
        binding->itemIndex = aboutIndex;
        binding->enabledState = itemDef->enabledState;
        itemDef->enabledState->deferBind(&ToolboxApp::MenuEnabledChangedThunk, binding);
        bindings_.push_back(binding);
      }
      InsertMenu(menu, 0);
      appMenuHandle = menu;
    }
    nextMenuId_ = 129;
  }

  if (!canPartial)
  {
    menuEntries_.clear();
    menuEntries_.resize(menuBar->menus.size());
    for (size_t i = 0; i < menuEntries_.size(); ++i)
    {
      menuEntries_[i].menu = 0;
      menuEntries_[i].menuId = 0;
      menuEntries_[i].isAppMenu = false;
    }
    for (size_t i = 0; i < menuBar->menus.size(); ++i)
    {
      const declara::app::MenuDefinition *menuDef = menuBar->menus[i];
      if (!menuDef)
        continue;
      if (menuDef->isAppMenu)
      {
        menuEntries_[i].menu = appMenuHandle;
        menuEntries_[i].menuId = appMenuHandle ? 128 : 0;
        menuEntries_[i].isAppMenu = true;
        menuEntries_[i].title = menuDef->title;
        continue;
      }
      Str255 title;
      CopyToPascalString(menuDef->title, title);
      if (title[0] == 0)
      {
        CopyToPascalString(loka::core::String::Literal("Menu"), title);
      }
      short menuId = nextMenuId_;
      MenuHandle menu = NewMenu(menuId, title);
      BuildMenuItems(menu, menuDef->itemsHead(), menuId, commands_, bindings_);
      if (CountMenuItems(menu) == 0)
      {
        DisposeMenu(menu);
        continue;
      }
      InsertMenu(menu, 0);
      menuEntries_[i].menu = menu;
      menuEntries_[i].menuId = menuId;
      menuEntries_[i].isAppMenu = false;
      menuEntries_[i].title = menuDef->title;
      ++nextMenuId_;
    }
    DrawMenuBar();
    clearMenuDiff();
    return;
  }

  bool needsFullRebuild = false;
  for (size_t index = 0; index < diff.changed.size(); ++index)
  {
    size_t i = diff.changed[index];
    if (i >= menuBar->menus.size())
    {
      needsFullRebuild = true;
      break;
    }
    const declara::app::MenuDefinition *menuDef = menuBar->menus[i];
    if (!menuDef)
    {
      needsFullRebuild = true;
      break;
    }
    MenuEntry &entry = menuEntries_[i];
    if (!entry.menu || entry.menuId == 0)
    {
      needsFullRebuild = true;
      break;
    }
    if (menuDef->isAppMenu && !entry.isAppMenu)
    {
      needsFullRebuild = true;
      break;
    }
    if (!menuDef->isAppMenu && entry.isAppMenu)
    {
      needsFullRebuild = true;
      break;
    }
    if (!menuDef->title.equals(entry.title))
    {
      needsFullRebuild = true;
      break;
    }
    clearMenuBindingsFor(entry.menu, entry.menuId);
    while (CountMenuItems(entry.menu) > 0)
    {
      DeleteMenuItem(entry.menu, 1);
    }
    if (menuDef->isAppMenu)
    {
      std::vector<declara::app::MenuItemDefinition *> aboutItems;
      for (size_t j = 0; j < menuBar->menus.size(); ++j)
      {
        const declara::app::MenuDefinition *appDef = menuBar->menus[j];
        if (!appDef || !appDef->isAppMenu)
          continue;
        const declara::app::MenuItemDefinition *itemDef = appDef->itemsHead();
        while (itemDef)
        {
          if (itemDef && itemDef->action == declara::app::MENU_ACTION_ABOUT_APP)
          {
            aboutItems.push_back(itemDef);
          }
          itemDef = itemDef->nextInComposition;
        }
      }
      if (aboutItems.empty())
      {
        needsFullRebuild = true;
        break;
      }
      AppendResMenu(entry.menu, 'DRVR');
      declara::app::MenuItemDefinition *itemDef = aboutItems[0];
      Str255 aboutTitle;
      CopyToPascalString(itemDef->title, aboutTitle);
      InsertMenuItem(entry.menu, aboutTitle, 0);
      short aboutIndex = 1;
      ToolboxApp::MenuCommand command;
      command.menuId = entry.menuId;
      command.itemIndex = aboutIndex;
      command.action = itemDef->action;
      command.emitter = itemDef->onClickState;
      commands_.push_back(command);
      if (itemDef->enabledState)
      {
        if (!itemDef->enabledState->get())
        {
          DisableItem(entry.menu, aboutIndex);
        }
        ToolboxApp::MenuBinding *binding = new ToolboxApp::MenuBinding();
        binding->menu = entry.menu;
        binding->itemIndex = aboutIndex;
        binding->enabledState = itemDef->enabledState;
        itemDef->enabledState->deferBind(&ToolboxApp::MenuEnabledChangedThunk, binding);
        bindings_.push_back(binding);
      }
      continue;
    }
    BuildMenuItems(entry.menu, menuDef->itemsHead(), entry.menuId, commands_, bindings_);
    if (CountMenuItems(entry.menu) == 0)
    {
      needsFullRebuild = true;
      break;
    }
  }
  if (needsFullRebuild)
  {
    resetMenuState();
    ClearMenuBar();
    InitMenus();
    applyMenuBar(activeWindow);
    return;
  }
  DrawMenuBar();
  clearMenuDiff();
}

void ToolboxApp::handleMenuCommand(short menuId, short item)
{
  if (menuId == 0 || item == 0)
    return;
  for (size_t i = 0; i < commands_.size(); ++i)
  {
    if (commands_[i].menuId != menuId || commands_[i].itemIndex != item)
      continue;
    switch (commands_[i].action)
    {
    case declara::app::MENU_ACTION_ABOUT_APP:
      SysBeep(1);
      return;
    case declara::app::MENU_ACTION_SHOW_COLOR_PICKER:
      SysBeep(1);
      return;
    case declara::app::MENU_ACTION_QUIT_APP:
      quit();
      return;
    case declara::app::MENU_ACTION_REBUILD_MENU:
      break;
    case declara::app::MENU_ACTION_NONE:
    default:
      break;
    }
    if (commands_[i].emitter)
    {
      commands_[i].emitter->emit();
    }
    if (commands_[i].action == declara::app::MENU_ACTION_REBUILD_MENU)
    {
      invalidateMenu();
    }
    return;
  }
}
