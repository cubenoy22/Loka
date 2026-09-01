#include "Win32LiveResizeTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <cwchar>
#include <windows.h>

#include "Win32BuiltInSupport.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "context/Win32PopupMenuContext.hpp"
#include "core/State.hpp"
#include "platform/String.hpp"

namespace
{
  const wchar_t *kLiveResizeHostClass = L"LOKA_LIVE_RESIZE_TEST_HOST";
  int gLiveResizeHostPaintCount = 0;

  class RefusingUtf8String : public loka::platform::String
  {
  public:
    explicit RefusingUtf8String(int *attempts)
        : attempts_(attempts)
    {
    }

    virtual bool appendUtf8(std::string &) const
    {
      ++*this->attempts_;
      return false;
    }

  private:
    int *attempts_;
  };

  LRESULT CALLBACK LiveResizeHostWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    if (message == WM_PAINT)
    {
      ++gLiveResizeHostPaintCount;
      PAINTSTRUCT paint;
      HDC hdc = BeginPaint(hwnd, &paint);
      RECT rect;
      GetClientRect(hwnd, &rect);
      FillRect(hdc, &rect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
      EndPaint(hwnd, &paint);
      return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  void ensureLiveResizeHostClass()
  {
    WNDCLASSW existing;
    if (GetClassInfoW(GetModuleHandleW(NULL), kLiveResizeHostClass, &existing))
    {
      return;
    }
    WNDCLASSW definition;
    ZeroMemory(&definition, sizeof(definition));
    definition.lpfnWndProc = LiveResizeHostWndProc;
    definition.hInstance = GetModuleHandleW(NULL);
    definition.lpszClassName = kLiveResizeHostClass;
    definition.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    LOKA_VERIFY(RegisterClassW(&definition));
  }

  HWND createLiveResizeHost()
  {
    ensureLiveResizeHostClass();
    HWND hwnd = CreateWindowExW(0,
                                kLiveResizeHostClass,
                                L"live-resize-test-host",
                                WS_POPUP,
                                -10000,
                                -10000,
                                400,
                                260,
                                NULL,
                                NULL,
                                GetModuleHandleW(NULL),
                                NULL);
    LOKA_VERIFY(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);
    return hwnd;
  }

  RECT childRectInParent(HWND child, HWND parent)
  {
    RECT rect;
    LOKA_VERIFY(GetWindowRect(child, &rect));
    MapWindowPoints(NULL, parent, reinterpret_cast<POINT *>(&rect), 2);
    return rect;
  }
} // namespace

// #549: every Text relayout used to synchronously redraw its parent subtree.
// Two Text children therefore presented two incomplete layouts during one
// root resize. The native layout pass positions every child without repaint,
// then presents the completed root exactly once.
void testWin32LayoutPresentsRootOnceAfterPositioningChildren()
{
  std::printf("\n==== [testWin32LayoutPresentsRootOnceAfterPositioningChildren] start ====\n");
  HWND root = createLiveResizeHost();
  {
    Win32ScenePlatformController controller(root);
    RegisterWin32BuiltInSupport(controller);
    loka::app::ColumnNode column((loka::app::ColumnProps()));
    column.addChild(new loka::app::TextNode((loka::app::TextProps("first"))));
    column.addChild(new loka::app::TextNode((loka::app::TextProps("second"))));

    controller.onChange(&column, loka::app::scene::NODE_DIRTY_NONE, false);
    controller.relayout(360, 220);
    gLiveResizeHostPaintCount = 0;
    controller.relayout(520, 300);

    assert(gLiveResizeHostPaintCount == 1 && "one native layout pass must present one completed root frame");
    controller.onChange(0, loka::app::scene::NODE_DIRTY_NONE, false);
  }
  LOKA_VERIFY(DestroyWindow(root));
  std::printf("==== [testWin32LayoutPresentsRootOnceAfterPositioningChildren] PASSED ====\n");
}

// #549: geometry refresh used to reset and repopulate every ComboBox item,
// then reapply selection. Item data is native identity that survives a pure
// geometry change only when relayout stops rebuilding the content channel.
void testWin32PopupRelayoutPreservesNativeItems()
{
  std::printf("\n==== [testWin32PopupRelayoutPreservesNativeItems] start ====\n");
  HWND root = createLiveResizeHost();
  {
    Win32ScenePlatformController controller(root);
    const char *itemLiterals[] = {"Apple", "Banana", "Cherry"};
    loka::core::MutableState<int> selection(1);
    int materializeAttempts = 0;
    loka::app::PopupMenuProps props;
    props.items(itemLiterals, 3).selectedIndex(&selection);
    loka::app::PopupMenuNode node(props);
    Win32PopupMenuContext context(&controller, root, 10, 10, 120, 24, &node);
    HWND popup = context.hwnd();
    LOKA_VERIFY(popup);
    LOKA_VERIFY(SendMessageW(popup, CB_GETCOUNT, 0, 0) == 3);
    const LPARAM itemIdentity = static_cast<LPARAM>(0x549);
    LOKA_VERIFY(SendMessageW(popup, CB_SETITEMDATA, 0, itemIdentity) != CB_ERR);

    context.relayout(30, 40, 180, 24);

    const RECT rect = childRectInParent(popup, root);
    assert(rect.left == 30 && rect.top == 40 && rect.right - rect.left == 180);
    LOKA_VERIFY(SendMessageW(popup, CB_GETCOUNT, 0, 0) == 3);
    LOKA_VERIFY(SendMessageW(popup, CB_GETITEMDATA, 0, 0) == itemIdentity);
    LOKA_VERIFY(SendMessageW(popup, CB_GETCURSEL, 0, 0) == 1);

    const char *replacementLiterals[] = {"One", "Two"};
    node.props.items(replacementLiterals, 2);
    context.relayout(30, 40, 180, 24);
    LOKA_VERIFY(SendMessageW(popup, CB_GETCOUNT, 0, 0) == 2);
    wchar_t firstItem[16];
    LOKA_VERIFY(SendMessageW(popup, CB_GETLBTEXT, 0, reinterpret_cast<LPARAM>(firstItem)) != CB_ERR);
    assert(std::wcscmp(firstItem, L"One") == 0);
    LOKA_VERIFY(SendMessageW(popup, CB_GETCURSEL, 0, 0) == 1);

    loka::Vector<loka::core::String> failingItems;
    failingItems.push_back(loka::core::String::FromPlatform(
        loka::core::Managed<loka::platform::String>::Wrap(new RefusingUtf8String(&materializeAttempts))));
    node.props.items(failingItems);
    context.relayout(30, 40, 180, 24);
    context.relayout(30, 40, 180, 24);
    assert(materializeAttempts == 2 && "failed native content application must retry on the next relayout");

    context.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    controller.drainNativeRetirements();
  }
  LOKA_VERIFY(DestroyWindow(root));
  std::printf("==== [testWin32PopupRelayoutPreservesNativeItems] PASSED ====\n");
}
