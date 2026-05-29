#include "Win32App.hpp"
#include "Win32Window.hpp" // Win32Window クラスの定義をインクルード
#include <windows.h>
#include <commdlg.h>
#include "app/core/App.hpp"
#include "platform/StringUTF8.hpp"

Win32App::Win32App(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow)
    : App(config),
      hInstance_(hInstance),
      nCmdShow_(nCmdShow),
      nextCommandId_(1000),
      activeMenu_(NULL),
      commands_(),
      bindings_()
{
  // App基底クラスですでにconfig_が初期化されているのでここでは不要
}

// デストラクタ: override を追加
Win32App::~Win32App()
{
  // windowsベクター内のウィンドウは App::windowClosed で削除されるか、
  // App のデストラクタで解放されるべき (App.hppに仮想デストラクタ追加済み)
  clearMenuBindings();
  if (activeMenu_)
  {
    DestroyMenu(activeMenu_);
    activeMenu_ = NULL;
  }
}

// アプリケーション終了処理 (Appからoverride)
void Win32App::quit()
{
  PostQuitMessage(0); // Win32のメッセージループを終了させる
}

// ウィンドウが閉じたときの処理 (Appからoverride)
void Win32App::windowClosed(Window *window)
{
  // まず基底クラスの処理を呼ぶ (windowsベクターからの削除と終了判定)
  App::windowClosed(window);

  // Win32固有の後処理があればここに追加
}

void Win32App::run()
{
  App::run();

  // 各ウィンドウにこのアプリインスタンスへの参照を設定する
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
      elapsedSeconds = static_cast<double>(now.QuadPart - lastTick.QuadPart) /
                       static_cast<double>(frequency.QuadPart);
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
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    return;
  }
  if (itemDef->action == loka::app::MENU_ACTION_SHOW_COLOR_PICKER)
  {
    return;
  }

  std::string titleUtf8;
  loka::platform::CollectUtf8(itemDef->title, titleUtf8);
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
    AppendMenuA(menu, flags | MF_POPUP, reinterpret_cast<UINT_PTR>(subMenu), titleUtf8.c_str());
    return;
  }

  int commandId = nextCommandId_++;
  AppendMenuA(menu, flags, static_cast<UINT_PTR>(commandId), titleUtf8.c_str());
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

void Win32App::buildMenuItems(HMENU menu,
                              const loka::app::MenuItemDefinition *itemsHead,
                              HWND hwnd)
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
  clearMenuBindings();
  if (activeMenu_)
  {
    DestroyMenu(activeMenu_);
    activeMenu_ = NULL;
  }

  Win32Window *win = activeWindow ? activeWindow->asWin32Window() : 0;
  HWND hwnd = win ? win->hwnd() : NULL;
  if (!hwnd)
  {
    return;
  }

  const loka::app::MenuBarDefinition *menuBar = resolveMenuBar(activeWindow);
  if (!menuBar)
  {
    SetMenu(hwnd, NULL);
    DrawMenuBar(hwnd);
    return;
  }

  HMENU menuBarHandle = CreateMenu();
  loka::dsl::CompositionCursor<loka::app::MenuDefinition> it(menuBar->menusHead(), menuBar->menusCount());
  for (loka::app::MenuDefinition *menuDef = it.next(); menuDef; menuDef = it.next())
  {
    if (menuDef->isAppMenu)
      continue;
    std::string titleUtf8;
    loka::platform::CollectUtf8(menuDef->title, titleUtf8);
    if (titleUtf8.empty())
      titleUtf8 = "Menu";
    HMENU subMenu = CreatePopupMenu();
    buildMenuItems(subMenu, menuDef->itemsHead(), hwnd);
    if (GetMenuItemCount(subMenu) == 0)
    {
      DestroyMenu(subMenu);
      continue;
    }
    AppendMenuA(menuBarHandle, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(subMenu), titleUtf8.c_str());
  }

  SetMenu(hwnd, menuBarHandle);
  DrawMenuBar(hwnd);
  activeMenu_ = menuBarHandle;
  clearMenuDiff();
}
