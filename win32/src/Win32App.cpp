#include "Win32App.hpp"
#include "Win32Window.hpp"
#include <windows.h>
#include <commdlg.h>
#include "app/core/App.hpp"
#include "platform/Win32String.hpp"

namespace
{
  bool ApplyWindowMenuPreservingContentFrame(Win32Window *window, HMENU menu)
  {
    if (!window || !window->hwnd())
    {
      return false;
    }
    loka::core::Frame contentFrame;
    if (!window->queryNativeContentFrame(contentFrame))
    {
      return false;
    }
    if (!SetMenu(window->hwnd(), menu))
    {
      return false;
    }
    window->applyNativeContentFrame(contentFrame);
    window->storeCurrentNativeContentFrame();
    DrawMenuBar(window->hwnd());
    return true;
  }

} // namespace

Win32App::AttachedMenu::AttachedMenu()
    : window_(0),
      menu_(NULL)
{
}

Win32App::AttachedMenu::~AttachedMenu()
{
  this->resetForTeardown();
}

bool Win32App::AttachedMenu::detach(DetachMode mode)
{
  if (!this->menu_)
  {
    this->window_ = 0;
    return true;
  }
  if (!this->window_ || !this->window_->hwnd())
  {
    // DestroyWindow owns an attached menu. A missing HWND means the native
    // window has already released this handle for us.
    this->window_ = 0;
    this->menu_ = NULL;
    return true;
  }
  if (GetMenu(this->window_->hwnd()) == this->menu_)
  {
    const bool detached = mode == DETACH_PRESERVING_CONTENT_FRAME
                              ? ApplyWindowMenuPreservingContentFrame(this->window_, NULL)
                              : this->window_->detachMenuForTeardown(this->menu_);
    if (!detached)
    {
      return false;
    }
  }
  this->window_ = 0;
  return true;
}

void Win32App::AttachedMenu::destroyDetached()
{
  if (this->menu_)
  {
    DestroyMenu(this->menu_);
    this->menu_ = NULL;
  }
}

bool Win32App::AttachedMenu::reset(DetachMode mode)
{
  if (!this->detach(mode))
  {
    return false;
  }
  this->destroyDetached();
  return true;
}

bool Win32App::AttachedMenu::resetPreservingContentFrame()
{
  return this->reset(DETACH_PRESERVING_CONTENT_FRAME);
}

bool Win32App::AttachedMenu::resetForTeardown()
{
  return this->reset(DETACH_FOR_TEARDOWN);
}

void Win32App::AttachedMenu::attach(Win32Window *window, HMENU menu)
{
  assert(!this->window_ && !this->menu_ && "AttachedMenu must be empty before attach");
  this->window_ = window;
  this->menu_ = menu;
}

Win32App::Win32App(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow)
    : App(config),
      hInstance_(hInstance),
      nCmdShow_(nCmdShow),
      nextCommandId_(1000),
      activeMenu_(),
      commands_(),
      bindings_()
{
  // App already owns the shared configuration state.
}

Win32App::~Win32App()
{
  this->activeMenu_.resetForTeardown();
  this->clearMenuBindings();
}

void Win32App::quit()
{
  PostQuitMessage(0);
}

void Win32App::run()
{
  App::run();

  // Give each Win32 window a back-reference for native callbacks.
  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      Window *w = (*it)->asWindow();
      Win32Window *win32Win = w ? w->asWin32Window() : 0;
      if (win32Win)
      {
        win32Win->setApp(this);
      }
    }
  }

  LARGE_INTEGER frequency;
  LARGE_INTEGER lastTick;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&lastTick);

  bool running = true;
  while (running)
  {
    MSG msg;
    bool handledMessage = false;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      handledMessage = true;
      if (msg.message == WM_QUIT)
      {
        running = false;
        break;
      }
      HWND root = msg.hwnd ? GetAncestor(msg.hwnd, GA_ROOT) : NULL;
      if (root && IsDialogMessage(root, &msg))
      {
        continue;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (!running)
    {
      break;
    }

    const loka::app::IdlePolicy policy = this->idlePolicy();

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsedSeconds = 0.0;
    if (frequency.QuadPart > 0)
    {
      elapsedSeconds = static_cast<double>(now.QuadPart - lastTick.QuadPart) / static_cast<double>(frequency.QuadPart);
    }
    lastTick = now;

    if (policy.mode == loka::app::IDLE_MODE_NONE)
    {
      this->flushMenuInvalidation();
      this->flushWindowInvalidations();
      if (!handledMessage)
      {
        WaitMessage();
      }
      continue;
    }

    double dispatchElapsedSeconds = 0.0;
    if (this->consumeIdle(elapsedSeconds, dispatchElapsedSeconds))
    {
      this->handleIdle(dispatchElapsedSeconds);
    }
    this->flushMenuInvalidation();
    this->flushWindowInvalidations();
    Sleep(1);
  }
}

bool Win32App::handleMenuCommand(int commandId, Window *window)
{
  (void)window;
  for (size_t i = 0; i < commands_.size(); ++i)
  {
    if (commands_[i].commandId != commandId)
      continue;
    switch (commands_[i].action)
    {
    case loka::app::MENU_ACTION_ABOUT_APP:
      if (commands_[i].emitter)
      {
        commands_[i].emitter->emit();
      }
      return true;
    case loka::app::MENU_ACTION_QUIT_APP:
      quit();
      return true;
    case loka::app::MENU_ACTION_REBUILD_MENU:
      break;
    case loka::app::MENU_ACTION_NONE:
    default:
      break;
    }
    if (commands_[i].emitter)
    {
      commands_[i].emitter->emit();
      if (commands_[i].action == loka::app::MENU_ACTION_REBUILD_MENU)
      {
        invalidateMenu();
      }
      return true;
    }
    if (commands_[i].action == loka::app::MENU_ACTION_REBUILD_MENU)
    {
      invalidateMenu();
      return true;
    }
    return false;
  }
  return false;
}

void Win32App::MenuEnabledChangedThunk(void *userData)
{
  MenuBinding *binding = static_cast<MenuBinding *>(userData);
  if (!binding || !binding->enabledState || !binding->menu)
    return;
  bool enabled = binding->enabledState->get();
  if (binding->invertEnabled)
  {
    enabled = !enabled;
  }
  EnableMenuItem(binding->menu, binding->commandId, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
  if (binding->hwnd)
  {
    DrawMenuBar(binding->hwnd);
  }
}

void Win32App::clearMenuBindings()
{
  for (size_t i = 0; i < bindings_.size(); ++i)
  {
    MenuBinding *binding = bindings_[i];
    if (binding && binding->enabledState)
    {
      binding->enabledState->deferUnbind(&Win32App::MenuEnabledChangedThunk, binding);
    }
    delete binding;
  }
  bindings_.clear();
  commands_.clear();
  nextCommandId_ = 1000;
}

void Win32App::buildMenuItem(HMENU menu, const loka::app::MenuItemDefinition *itemDef, HWND hwnd)
{
  if (!itemDef)
    return;
  if (!itemDef->isVisibleInitial())
  {
    return;
  }
  if (itemDef->isSeparator)
  {
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    return;
  }
  if (itemDef->action == loka::app::MENU_ACTION_SHOW_COLOR_PICKER)
  {
    return;
  }

  std::wstring titleWide;
  loka::win32::MaterializeWideString(itemDef->title, titleWide);
  UINT flags = MF_STRING;
  if (!itemDef->isEnabledInitial())
  {
    flags |= MF_GRAYED;
  }

  if (itemDef->hasChildren())
  {
    HMENU subMenu = CreatePopupMenu();
    buildMenuItems(subMenu, itemDef->childrenHead(), hwnd);
    if (GetMenuItemCount(subMenu) == 0)
    {
      DestroyMenu(subMenu);
      return;
    }
    AppendMenuW(menu, flags | MF_POPUP, reinterpret_cast<UINT_PTR>(subMenu), titleWide.c_str());
    return;
  }

  int commandId = nextCommandId_++;
  AppendMenuW(menu, flags, static_cast<UINT_PTR>(commandId), titleWide.c_str());
  Win32App::MenuCommand command;
  command.commandId = commandId;
  command.action = itemDef->action;
  command.emitter = itemDef->onClickState;
  commands_.push_back(command);
  loka::core::State<bool> *enabledBindingState = itemDef->enabledBindingState();
  if (enabledBindingState)
  {
    Win32App::MenuBinding *binding = new Win32App::MenuBinding();
    binding->menu = menu;
    binding->commandId = commandId;
    binding->hwnd = hwnd;
    binding->enabledState = enabledBindingState;
    binding->invertEnabled = itemDef->enabledBindingInvert();
    binding->enabledState->deferBind(&Win32App::MenuEnabledChangedThunk, binding);
    bindings_.push_back(binding);
  }
}

void Win32App::buildMenuItems(HMENU menu, const loka::app::MenuItemDefinition *itemsHead, HWND hwnd)
{
  const loka::app::MenuItemDefinition *itemDef = itemsHead;
  while (itemDef)
  {
    buildMenuItem(menu, itemDef, hwnd);
    itemDef = itemDef->nextInComposition;
  }
}

void Win32App::applyMenuBar(Window *activeWindow)
{
  if (!this->activeMenu_.resetPreservingContentFrame())
  {
    return;
  }
  this->clearMenuBindings();

  Win32Window *win = activeWindow ? activeWindow->asWin32Window() : 0;
  HWND hwnd = win ? win->hwnd() : NULL;
  if (!hwnd)
  {
    return;
  }

  const loka::app::MenuBarDefinition *menuBar = resolveMenuBar(activeWindow);
  if (!menuBar)
  {
    ApplyWindowMenuPreservingContentFrame(win, NULL);
    return;
  }

  HMENU menuBarHandle = CreateMenu();
  loka::dsl::CompositionCursor<loka::app::MenuDefinition> it(menuBar->menusHead(), menuBar->menusCount());
  for (loka::app::MenuDefinition *menuDef = it.next(); menuDef; menuDef = it.next())
  {
    if (menuDef->isAppMenu)
      continue;
    std::wstring titleWide;
    loka::win32::MaterializeWideString(menuDef->title, titleWide);
    if (titleWide.empty())
      titleWide = L"Menu";
    HMENU subMenu = CreatePopupMenu();
    buildMenuItems(subMenu, menuDef->itemsHead(), hwnd);
    if (GetMenuItemCount(subMenu) == 0)
    {
      DestroyMenu(subMenu);
      continue;
    }
    AppendMenuW(menuBarHandle, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(subMenu), titleWide.c_str());
  }

  // AppMenu is platform-reserved and skipped above. An empty HMENU draws no
  // menu row, so attaching one would disagree with the frame conversion.
  if (GetMenuItemCount(menuBarHandle) == 0)
  {
    DestroyMenu(menuBarHandle);
    if (ApplyWindowMenuPreservingContentFrame(win, NULL))
    {
      clearMenuDiff();
    }
    return;
  }

  if (!ApplyWindowMenuPreservingContentFrame(win, menuBarHandle))
  {
    DestroyMenu(menuBarHandle);
    return;
  }
  this->activeMenu_.attach(win, menuBarHandle);
  clearMenuDiff();
}
