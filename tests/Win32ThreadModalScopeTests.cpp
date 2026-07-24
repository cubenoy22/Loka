#include "Win32ThreadModalScopeTests.hpp"
#include <cassert>
#include <cstdio>
#include "context/Win32ThreadModalDialogScope.hpp"

namespace
{
  HWND createTopLevelProbeWindow(const wchar_t *title)
  {
    return CreateWindowExW(
        0, L"STATIC", title, WS_OVERLAPPED, 0, 0, 40, 30, NULL, NULL, GetModuleHandle(NULL), NULL);
  }
} // namespace

void testWin32ThreadModalScopeHoldsSiblingsDisabled()
{
  printf("\n==== [testWin32ThreadModalScopeHoldsSiblingsDisabled] start ====\n");
  // #152 trigger guard: the scope must disable every sibling top-level window
  // for its lifetime and restore exactly the set it disabled.
  HWND owner = createTopLevelProbeWindow(L"modal-owner");
  HWND sibling = createTopLevelProbeWindow(L"modal-sibling");
  HWND preDisabled = createTopLevelProbeWindow(L"modal-predisabled");
  assert(owner && sibling && preDisabled);
  EnableWindow(preDisabled, FALSE);

  assert(IsWindowEnabled(owner));
  assert(IsWindowEnabled(sibling));
  assert(!IsWindowEnabled(preDisabled));

  {
    loka::win32::ThreadModalDialogScope outer(owner);
    assert(IsWindowEnabled(owner) && "the owner stays under the common dialog's own control");
    assert(!IsWindowEnabled(sibling) && "siblings must not accept input while the dialog pumps");
    assert(!IsWindowEnabled(preDisabled));

    {
      loka::win32::ThreadModalDialogScope inner(owner);
      assert(!IsWindowEnabled(sibling));
    }
    assert(!IsWindowEnabled(sibling) && "the inner scope restores only what it disabled");
  }

  assert(IsWindowEnabled(sibling) && "siblings are re-enabled when the dialog scope ends");
  assert(!IsWindowEnabled(preDisabled) && "a window disabled before the scope stays disabled");

  DestroyWindow(preDisabled);
  DestroyWindow(sibling);
  DestroyWindow(owner);
  printf("==== [testWin32ThreadModalScopeHoldsSiblingsDisabled] end ====\n");
}
