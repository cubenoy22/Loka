#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"
#include "ToolboxScenePlatformController.hpp"

#include <Dialogs.h>
#include <Events.h>
#include <Fonts.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <Sound.h>
#include <TextEdit.h>
#include <Windows.h>
#include <Devices.h>
#include "loka/platform/StringUTF8.hpp"

ToolboxApp::ToolboxApp(AppConfigurable *config)
    : App(config), nextMenuId_(128), commands_(), bindings_(), menuEntries_(), hierarchicalMenus_(), pendingWindowClosures_(), running_(false)
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
  applyMenuBar(0);
  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    ToolboxWindow *firstWindow = 0;
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      Window *w = (*it)->asWindow();
      ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
      if (toolboxWindow)
      {
        if (!firstWindow)
        {
          firstWindow = toolboxWindow;
        }
        toolboxWindow->setApp(this);
        toolboxWindow->open();
        toolboxWindow->ensureSceneMounted();
        toolboxWindow->draw();
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
    flushPendingWindowClosures();
    this->flushMenuInvalidation();
    this->flushWindowInvalidations();
    EventRecord event;
    WaitNextEvent(everyEvent, &event, 1, 0);
    if (event.what == nullEvent && group_)
    {
      const std::vector<AppComponent *> &comps = group_->getComponents();
      for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
      {
        Window *w = (*it)->asWindow();
        ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
        if (toolboxWindow)
        {
          toolboxWindow->flushInvalidate();
        }
      }
      for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
      {
        Window *w = (*it)->asWindow();
        ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
        if (toolboxWindow)
        {
          toolboxWindow->flushDeferredDebugDump();
        }
      }
    }
    // TODO: Re-enable invalidation once Classic update flow is stable.
    if (group_)
    {
      const std::vector<AppComponent *> &comps = group_->getComponents();
      for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
      {
        Window *w = (*it)->asWindow();
        ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
        if (toolboxWindow)
        {
          toolboxWindow->idleControls();
        }
      }
    }
    ToolboxWindow *active = activeWindow() ? activeWindow()->asToolboxWindow() : 0;
    if (active)
    {
      active->updateCursor();
    }
    if (event.what == updateEvt)
    {
      WindowPtr target = reinterpret_cast<WindowPtr>(event.message);
      if (target && group_)
      {
        const std::vector<AppComponent *> &comps = group_->getComponents();
        for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
        {
          Window *w = (*it)->asWindow();
          ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
          if (toolboxWindow && toolboxWindow->window() == target)
          {
            if (toolboxWindow->shouldSkipNextUpdateDraw())
            {
              toolboxWindow->clearSkipNextUpdateDraw();
              break;
            }
            if (toolboxWindow->scenePlatformController())
            {
              toolboxWindow->scenePlatformController()->noteWindowUpdateEvtDraw();
            }
            BeginUpdate(target);
            toolboxWindow->draw();
            EndUpdate(target);
            break;
          }
        }
      }
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
              Window *w = (*it)->asWindow();
              ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
              if (toolboxWindow && toolboxWindow->window() == target)
              {
                closing = toolboxWindow;
                break;
              }
            }
          }
          if (closing)
          {
            requestWindowClose(closing);
          }
        }
      }
      else if (part == inContent && target)
      {
        ToolboxWindow *clicked = 0;
        if (group_)
        {
          const std::vector<AppComponent *> &comps = group_->getComponents();
          for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
          {
            Window *w = (*it)->asWindow();
            ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
            if (toolboxWindow && toolboxWindow->window() == target)
            {
              clicked = toolboxWindow;
              break;
            }
          }
        }
        if (clicked)
        {
          if (clicked->hasPendingInvalidate())
          {
            continue;
          }
          bool inEdit = clicked->handleMouseDown(event.where);
          if (inEdit)
          {
            CursHandle ibeam = GetCursor(iBeamCursor);
            if (ibeam)
            {
              SetCursor(*ibeam);
            }
          }
          else
          {
            InitCursor();
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
          Window *w = (*it)->asWindow();
          ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
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
      ToolboxWindow *active = activeWindow() ? activeWindow()->asToolboxWindow() : 0;
      if (active)
      {
        char key = static_cast<char>(event.message & charCodeMask);
        if (active->handleKeyDown(key))
        {
          continue;
        }
      }
      long choice = MenuKey(static_cast<char>(event.message & charCodeMask));
      if (choice != 0)
      {
        short menuId = static_cast<short>(choice >> 16);
        short item = static_cast<short>(choice & 0xFFFF);
        handleMenuCommand(menuId, item);
        HiliteMenu(0);
      }
    }
    flushPendingWindowClosures();
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

void ToolboxApp::requestWindowClose(Window *window)
{
  if (!window)
  {
    return;
  }
  for (size_t i = 0; i < pendingWindowClosures_.size(); ++i)
  {
    if (pendingWindowClosures_[i] == window)
    {
      return;
    }
  }
  pendingWindowClosures_.push_back(window);
}

void ToolboxApp::flushPendingWindowClosures()
{
  if (pendingWindowClosures_.empty())
  {
    return;
  }
  std::vector<Window *> pending = pendingWindowClosures_;
  pendingWindowClosures_.clear();
  for (size_t i = 0; i < pending.size(); ++i)
  {
    Window *window = pending[i];
    windowClosed(window);
  }
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
  bool enabled = binding->enabledState->get();
  if (binding->invertEnabled)
  {
    enabled = !enabled;
  }
  if (enabled)
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
    if (binding)
    {
      binding->menu = 0;
      binding->enabledState = 0;
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
      binding->menu = 0;
      binding->enabledState = 0;
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

void ToolboxApp::disposeHierarchicalMenus()
{
  for (size_t i = 0; i < hierarchicalMenus_.size(); ++i)
  {
    if (hierarchicalMenus_[i])
    {
      DisposeMenu(hierarchicalMenus_[i]);
    }
  }
  hierarchicalMenus_.clear();
}

void ToolboxApp::resetMenuState()
{
  clearMenuBindings();
  disposeHierarchicalMenus();
  disposeMenuEntries();
  nextMenuId_ = 128;
}

static void BuildMenuItems(MenuHandle menu,
                           const loka::app::MenuItemDefinition *itemsHead,
                           short menuId,
                           short &nextMenuId,
                           std::vector<ToolboxApp::MenuCommand> &commands,
                           std::vector<ToolboxApp::MenuBinding *> &bindings,
                           std::vector<MenuHandle> &hierarchicalMenus)
{
  const loka::app::MenuItemDefinition *itemDef = itemsHead;
  while (itemDef)
  {
    if (!itemDef)
    {
      itemDef = itemDef->nextInComposition;
      continue;
    }
    if (!itemDef->isVisibleInitial())
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
    if (itemDef->action == loka::app::MENU_ACTION_SHOW_COLOR_PICKER)
    {
      itemDef = itemDef->nextInComposition;
      continue;
    }
    Str255 title;
    CopyToPascalString(itemDef->title, title);
    AppendMenu(menu, title);
    short itemIndex = CountMenuItems(menu);
    if (itemDef->hasChildren())
    {
      short subMenuId = nextMenuId++;
      MenuHandle subMenu = NewMenu(subMenuId, title);
      BuildMenuItems(subMenu, itemDef->childrenHead(), subMenuId, nextMenuId, commands, bindings, hierarchicalMenus);
      if (CountMenuItems(subMenu) == 0)
      {
        DisposeMenu(subMenu);
        DeleteMenuItem(menu, itemIndex);
        itemDef = itemDef->nextInComposition;
        continue;
      }
      InsertMenu(subMenu, kInsertHierarchicalMenu);
      SetItemCmd(menu, itemIndex, hMenuCmd);
      OSErr hierErr = SetMenuItemHierarchicalID(menu, itemIndex, subMenuId);
      if (hierErr != noErr)
      {
        SetItemMark(menu, itemIndex, static_cast<CharParameter>(subMenuId & 0xFF));
      }
      hierarchicalMenus.push_back(subMenu);
      itemDef = itemDef->nextInComposition;
      continue;
    }
    ToolboxApp::MenuCommand command;
    command.menuId = menuId;
    command.itemIndex = itemIndex;
    command.action = itemDef->action;
    command.emitter = itemDef->onClickState;
    commands.push_back(command);
    if (!itemDef->isEnabledInitial())
    {
      DisableItem(menu, itemIndex);
    }
    loka::core::State<bool> *enabledBindingState = itemDef->enabledBindingState();
    if (enabledBindingState)
    {
      ToolboxApp::MenuBinding *binding = new ToolboxApp::MenuBinding();
      binding->menu = menu;
      binding->itemIndex = itemIndex;
      binding->enabledState = enabledBindingState;
      binding->invertEnabled = itemDef->enabledBindingInvert();
      enabledBindingState->deferBind(&ToolboxApp::MenuEnabledChangedThunk, binding);
      bindings.push_back(binding);
    }
    itemDef = itemDef->nextInComposition;
  }
}

static bool HasHierarchicalItems(const loka::app::MenuItemDefinition *itemsHead)
{
  const loka::app::MenuItemDefinition *itemDef = itemsHead;
  while (itemDef)
  {
    if (itemDef->hasChildren())
    {
      return true;
    }
    if (HasHierarchicalItems(itemDef->childrenHead()))
    {
      return true;
    }
    itemDef = itemDef->nextInComposition;
  }
  return false;
}

void ToolboxApp::applyMenuBar(Window *activeWindow)
{
  const loka::app::MenuBarDefinition *menuBar = resolveMenuBar(activeWindow);
  if (!menuBar)
  {
    resetMenuState();
    ClearMenuBar();
    InitMenus();
    DrawMenuBar();
    if (activeWindow && activeWindow->asToolboxWindow())
    {
      activeWindow->asToolboxWindow()->refreshFrame();
    }
    clearMenuDiff();
    return;
  }

  size_t menuCount = menuBar->menusCount();
  bool forceFullRebuild = (activeWindow && activeWindow->menuBar());
  const loka::app::MenuCompositionDiff &diff = menuDiff();
  if (!diff.valid && !forceFullRebuild)
  {
    return;
  }
  bool canPartial = diff.valid && !diff.fullRebuild && !forceFullRebuild;
  bool hasHierarchical = false;
  loka::dsl::CompositionCursor<loka::app::MenuDefinition> hierarchyIt(menuBar->menusHead(), menuCount);
  for (loka::app::MenuDefinition *menuDef = hierarchyIt.next(); menuDef; menuDef = hierarchyIt.next())
  {
    if (HasHierarchicalItems(menuDef->itemsHead()))
    {
      hasHierarchical = true;
      break;
    }
  }
  if (hasHierarchical || !hierarchicalMenus_.empty())
  {
    forceFullRebuild = true;
    canPartial = false;
  }
  if (canPartial && menuEntries_.size() != menuCount)
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
  loka::dsl::CompositionCursor<loka::app::MenuDefinition> appMenuScan(menuBar->menusHead(), menuCount);
  for (loka::app::MenuDefinition *menuDef = appMenuScan.next(); menuDef; menuDef = appMenuScan.next())
  {
    if (menuDef->isAppMenu)
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
    std::vector<const loka::app::MenuItemDefinition *> aboutItems;
    loka::dsl::CompositionCursor<loka::app::MenuDefinition> appMenuIt(menuBar->menusHead(), menuCount);
    for (loka::app::MenuDefinition *menuDef = appMenuIt.next(); menuDef; menuDef = appMenuIt.next())
    {
      if (!menuDef->isAppMenu)
        continue;
      const loka::app::MenuItemDefinition *itemDef = menuDef->itemsHead();
      while (itemDef)
      {
        if (itemDef->action == loka::app::MENU_ACTION_ABOUT_APP)
        {
          if (itemDef->isVisibleInitial())
          {
            aboutItems.push_back(itemDef);
          }
        }
        itemDef = itemDef->nextInComposition;
      }
    }
    if (!aboutItems.empty())
    {
      AppendResMenu(menu, 'DRVR');
      const loka::app::MenuItemDefinition *itemDef = aboutItems[0];
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
      if (!itemDef->isEnabledInitial())
      {
        DisableItem(menu, aboutIndex);
      }
      loka::core::State<bool> *enabledBindingState = itemDef->enabledBindingState();
      if (enabledBindingState)
      {
        ToolboxApp::MenuBinding *binding = new ToolboxApp::MenuBinding();
        binding->menu = menu;
        binding->itemIndex = aboutIndex;
        binding->enabledState = enabledBindingState;
        binding->invertEnabled = itemDef->enabledBindingInvert();
        enabledBindingState->deferBind(&ToolboxApp::MenuEnabledChangedThunk, binding);
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
    menuEntries_.resize(menuCount);
    for (size_t i = 0; i < menuEntries_.size(); ++i)
    {
      menuEntries_[i].menu = 0;
      menuEntries_[i].menuId = 0;
      menuEntries_[i].isAppMenu = false;
    }
    size_t menuIndex = 0;
    loka::dsl::CompositionCursor<loka::app::MenuDefinition> it(menuBar->menusHead(), menuCount);
    for (loka::app::MenuDefinition *menuDef = it.next(); menuDef; menuDef = it.next(), ++menuIndex)
    {
      if (menuDef->isAppMenu)
      {
        menuEntries_[menuIndex].menu = appMenuHandle;
        menuEntries_[menuIndex].menuId = appMenuHandle ? 128 : 0;
        menuEntries_[menuIndex].isAppMenu = true;
        menuEntries_[menuIndex].title = menuDef->title;
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
      BuildMenuItems(menu, menuDef->itemsHead(), menuId, nextMenuId_, commands_, bindings_, hierarchicalMenus_);
      if (CountMenuItems(menu) == 0)
      {
        DisposeMenu(menu);
        continue;
      }
      InsertMenu(menu, 0);
      menuEntries_[menuIndex].menu = menu;
      menuEntries_[menuIndex].menuId = menuId;
      menuEntries_[menuIndex].isAppMenu = false;
      menuEntries_[menuIndex].title = menuDef->title;
      ++nextMenuId_;
    }
    DrawMenuBar();
    if (activeWindow && activeWindow->asToolboxWindow())
    {
      activeWindow->asToolboxWindow()->refreshFrame();
    }
    clearMenuDiff();
    return;
  }

  bool needsFullRebuild = false;
  loka::dsl::CompositionCursor<loka::app::MenuCompositionDiff::ChangedIndex> diffIt(
      diff.changedHead(), diff.changedCount());
  for (loka::app::MenuCompositionDiff::ChangedIndex *diffEntry = diffIt.next(); diffEntry; diffEntry = diffIt.next())
  {
    size_t i = diffEntry->value;
    if (i >= menuCount)
    {
      needsFullRebuild = true;
      break;
    }
    const loka::app::MenuDefinition *menuDef = menuBar->menuAt(i);
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
      std::vector<const loka::app::MenuItemDefinition *> aboutItems;
      loka::dsl::CompositionCursor<loka::app::MenuDefinition> appMenuIt(menuBar->menusHead(), menuCount);
      for (loka::app::MenuDefinition *appDef = appMenuIt.next(); appDef; appDef = appMenuIt.next())
      {
        if (!appDef->isAppMenu)
          continue;
        const loka::app::MenuItemDefinition *itemDef = appDef->itemsHead();
        while (itemDef)
        {
          if (itemDef->action == loka::app::MENU_ACTION_ABOUT_APP)
          {
            if (itemDef->isVisibleInitial())
            {
              aboutItems.push_back(itemDef);
            }
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
      const loka::app::MenuItemDefinition *itemDef = aboutItems[0];
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
      if (!itemDef->isEnabledInitial())
      {
        DisableItem(entry.menu, aboutIndex);
      }
      loka::core::State<bool> *enabledBindingState = itemDef->enabledBindingState();
      if (enabledBindingState)
      {
        ToolboxApp::MenuBinding *binding = new ToolboxApp::MenuBinding();
        binding->menu = entry.menu;
        binding->itemIndex = aboutIndex;
        binding->enabledState = enabledBindingState;
        binding->invertEnabled = itemDef->enabledBindingInvert();
        enabledBindingState->deferBind(&ToolboxApp::MenuEnabledChangedThunk, binding);
        bindings_.push_back(binding);
      }
      continue;
    }
    BuildMenuItems(entry.menu, menuDef->itemsHead(), entry.menuId, nextMenuId_, commands_, bindings_, hierarchicalMenus_);
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
  if (activeWindow && activeWindow->asToolboxWindow())
  {
    activeWindow->asToolboxWindow()->refreshFrame();
  }
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
    case loka::app::MENU_ACTION_ABOUT_APP:
      SysBeep(1);
      return;
    case loka::app::MENU_ACTION_SHOW_COLOR_PICKER:
      SysBeep(1);
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
      requestMenuInvalidation();
    }
    return;
  }
  if (menuId == 128)
  {
    Str255 deskAccName;
    GetMenuItemText(GetMenuHandle(menuId), item, deskAccName);
    OpenDeskAcc(deskAccName);
  }
}
