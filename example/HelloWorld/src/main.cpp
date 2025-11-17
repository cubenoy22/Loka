#include <windows.h>
#include <string>
#include <cassert>
#include "Win32App.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/PlatformContext.hpp"
#include "Win32PlatformContext.hpp"
#include "core/App.hpp"
#include "core/Window.hpp"
#include "core/SceneManager2.hpp"
#include "core/util/ScopedPtr.hpp"
#include "core/AppConfigurable.hpp"
#include "core/util/AutoTransactionGuard.hpp"
#include "core/util/StateUtil.hpp"
#include "core/components/logic/format.hpp"
#include "FormScene.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  Win32PlatformContext platformContext;
  MyAppConfig config(&platformContext);
  ScopedPtr<App>(platformContext.createApp(&config, hInstance, nCmdShow))
      ->run();
  return 0;
}

