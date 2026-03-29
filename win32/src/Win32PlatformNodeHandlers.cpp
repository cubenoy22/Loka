#include "Win32PlatformNodeHandlers.hpp"
#include "context/Win32ButtonContext.hpp"
#include "context/Win32CellContext.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32ImageViewContext.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include "context/Win32PopupMenuContext.hpp"
#include "context/Win32TextContext.hpp"

void RegisterWin32PlatformNodeHandlers(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  RegisterWin32ButtonNodeHandler(registry);
  RegisterWin32TextNodeHandler(registry);
  RegisterWin32ImageViewNodeHandler(registry);
  RegisterWin32EditTextNodeHandler(registry);
  RegisterWin32PopupMenuNodeHandler(registry);
  RegisterWin32CellNodeHandler(registry);
  RegisterWin32OpenFileDialogNodeHandler(registry);
}
