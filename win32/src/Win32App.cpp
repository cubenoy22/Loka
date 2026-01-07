#include "Win32App.hpp"
#include "Win32Window.hpp" // Win32Window クラスの定義をインクルード
#include <windows.h>
#include <commdlg.h>
#include "core/App.hpp"
#include "loka/platform/StringUTF8.hpp"

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
  // 基底クラスの実行
  App::run();

  // 各ウィンドウにこのアプリインスタンスへの参照を設定する
  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      Win32Window *win32Win = dynamic_cast<Win32Window *>(*it);
      if (win32Win)
      {
        win32Win->setApp(this);
      }
    }
  }

  // Win32メッセージループ
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0))
  {
    HWND root = msg.hwnd ? GetAncestor(msg.hwnd, GA_ROOT) : NULL;
    if (root && IsDialogMessage(root, &msg))
    {
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  // GetMessageが0を返したら (PostQuitMessageが呼ばれたら) ループを抜ける
  // run()メソッドが終了し、WinMainに戻る
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
    case declara::app::MENU_ACTION_ABOUT_APP:
      if (commands_[i].emitter)
      {
        commands_[i].emitter->emit();
      }
      return true;
    case declara::app::MENU_ACTION_QUIT_APP:
      quit();
      return true;
    case declara::app::MENU_ACTION_REBUILD_MENU:
      break;
    case declara::app::MENU_ACTION_NONE:
    default:
      break;
    }
    if (commands_[i].emitter)
    {
      commands_[i].emitter->emit();
      if (commands_[i].action == declara::app::MENU_ACTION_REBUILD_MENU)
      {
        invalidateMenu();
      }
      return true;
    }
    if (commands_[i].action == declara::app::MENU_ACTION_REBUILD_MENU)
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

void Win32App::buildMenuItem(HMENU menu, const declara::app::MenuItemDefinition *itemDef, HWND hwnd)
{
  if (!itemDef)
    return;
  if (itemDef->isSeparator)
  {
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    return;
  }
  if (itemDef->action == declara::app::MENU_ACTION_SHOW_COLOR_PICKER)
  {
    return;
  }

  std::string titleUtf8;
  loka::platform::CollectUtf8(itemDef->title, titleUtf8);
  UINT flags = MF_STRING;
  if (itemDef->enabledState && !itemDef->enabledState->get())
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
  if (itemDef->enabledState)
  {
    Win32App::MenuBinding *binding = new Win32App::MenuBinding();
    binding->menu = menu;
    binding->commandId = commandId;
    binding->hwnd = hwnd;
    binding->enabledState = itemDef->enabledState;
    binding->enabledState->deferBind(&Win32App::MenuEnabledChangedThunk, binding);
    bindings_.push_back(binding);
  }
}

void Win32App::buildMenuItems(HMENU menu,
                              const std::vector<declara::app::MenuItemDefinition *> &items,
                              HWND hwnd)
{
  for (size_t i = 0; i < items.size(); ++i)
  {
    buildMenuItem(menu, items[i], hwnd);
  }
}

void Win32App::buildMenuItems(HMENU menu,
                              const declara::app::MenuItemDefinition *itemsHead,
                              HWND hwnd)
{
  const declara::app::MenuItemDefinition *itemDef = itemsHead;
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

  Win32Window *win = dynamic_cast<Win32Window *>(activeWindow);
  HWND hwnd = win ? win->hwnd() : NULL;
  if (!hwnd)
  {
    return;
  }

  const declara::app::MenuBarDefinition *menuBar = resolveMenuBar(activeWindow);
  if (!menuBar)
  {
    SetMenu(hwnd, NULL);
    DrawMenuBar(hwnd);
    return;
  }

  HMENU menuBarHandle = CreateMenu();
  for (size_t i = 0; i < menuBar->menus.size(); ++i)
  {
    const declara::app::MenuDefinition *menuDef = menuBar->menus[i];
    if (!menuDef)
      continue;
    if (menuDef->isAppMenu)
      continue;
    std::string titleUtf8;
    loka::platform::CollectUtf8(menuDef->title, titleUtf8);
    if (titleUtf8.empty())
      titleUtf8 = "Menu";
    HMENU subMenu = CreatePopupMenu();
    buildMenuItems(subMenu, menuDef->items, hwnd);
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
