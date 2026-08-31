#include "Win32ScrollViewTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <cwchar>
#include <vector>
#include <windows.h>

#include "Win32BuiltInSupport.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"

namespace
{
  class OffsetFact
  {
  public:
    explicit OffsetFact(int initial)
        : storage_(initial),
          tracker_(),
          state_(&this->storage_, &this->tracker_),
          changeCount_(0)
    {
      this->tracker_.addState(&this->storage_);
      this->storage_.bind(&OffsetFact::ChangedThunk, this, false);
    }

    ~OffsetFact()
    {
      this->storage_.unbind(&OffsetFact::ChangedThunk, this);
    }

    loka::app::scene::NodeState<int> &state()
    {
      return this->state_;
    }

    int changeCount() const
    {
      return this->changeCount_;
    }

    void resetChangeCount()
    {
      this->changeCount_ = 0;
    }

  private:
    static void ChangedThunk(void *userData)
    {
      OffsetFact *self = static_cast<OffsetFact *>(userData);
      if (self)
      {
        ++self->changeCount_;
      }
    }

    loka::core::MutableState<int> storage_;
    loka::core::PushStateTracker tracker_;
    loka::app::scene::NodeState<int> state_;
    int changeCount_;

    OffsetFact(const OffsetFact &);
    OffsetFact &operator=(const OffsetFact &);
  };

  struct ClassWindowQuery
  {
    explicit ClassWindowQuery(const wchar_t *className)
        : className_(className),
          hwnd_(0)
    {
    }

    const wchar_t *className_;
    HWND hwnd_;
  };

  BOOL CALLBACK FindClassWindowThunk(HWND hwnd, LPARAM lParam)
  {
    ClassWindowQuery *query = reinterpret_cast<ClassWindowQuery *>(lParam);
    wchar_t className[64];
    const int length = GetClassNameW(
        hwnd, className, sizeof(className) / sizeof(className[0]));
    if (!query->hwnd_ && length > 0 &&
        std::wcscmp(className, query->className_) == 0)
    {
      query->hwnd_ = hwnd;
    }
    return TRUE;
  }

  HWND findChildWindowByClass(HWND root, const wchar_t *className)
  {
    ClassWindowQuery query(className);
    LOKA_VERIFY(EnumChildWindows(
        root, FindClassWindowThunk, reinterpret_cast<LPARAM>(&query)));
    return query.hwnd_;
  }

  struct DirectChildQuery
  {
    DirectChildQuery(HWND parent,
                     const wchar_t *className,
                     std::vector<HWND> &out)
        : parent_(parent),
          className_(className),
          out_(out)
    {
    }

    HWND parent_;
    const wchar_t *className_;
    std::vector<HWND> &out_;
  };

  BOOL CALLBACK CollectDirectChildrenThunk(HWND hwnd, LPARAM lParam)
  {
    DirectChildQuery *query = reinterpret_cast<DirectChildQuery *>(lParam);
    wchar_t className[64];
    const int length = GetClassNameW(
        hwnd, className, sizeof(className) / sizeof(className[0]));
    if (GetParent(hwnd) == query->parent_ && length > 0 &&
        std::wcscmp(className, query->className_) == 0)
    {
      query->out_.push_back(hwnd);
    }
    return TRUE;
  }

  std::vector<HWND> directChildWindowsByClass(HWND parent,
                                               const wchar_t *className)
  {
    std::vector<HWND> result;
    DirectChildQuery query(parent, className, result);
    LOKA_VERIFY(EnumChildWindows(
        parent, CollectDirectChildrenThunk, reinterpret_cast<LPARAM>(&query)));
    return result;
  }

  RECT childRectInParent(HWND child, HWND parent)
  {
    RECT rect;
    LOKA_VERIFY(GetWindowRect(child, &rect));
    POINT topLeft = {rect.left, rect.top};
    POINT bottomRight = {rect.right, rect.bottom};
    LOKA_VERIFY(ScreenToClient(parent, &topLeft));
    LOKA_VERIFY(ScreenToClient(parent, &bottomRight));
    RECT result = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    return result;
  }

  SCROLLINFO scrollInfo(HWND viewport)
  {
    SCROLLINFO info;
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    LOKA_VERIFY(GetScrollInfo(viewport, SB_VERT, &info));
    return info;
  }

  int maximumOffset(const SCROLLINFO &info)
  {
    int maximum = info.nMax;
    if (info.nPage > 0)
    {
      maximum -= static_cast<int>(info.nPage - 1);
    }
    return maximum < info.nMin ? info.nMin : maximum;
  }

  HWND createHostWindow()
  {
    return CreateWindowExW(0,
                           L"STATIC",
                           L"scroll-view-host",
                           WS_OVERLAPPED,
                           0,
                           0,
                           360,
                           280,
                           NULL,
                           NULL,
                           GetModuleHandleW(NULL),
                           NULL);
  }

  loka::app::ColumnNode *addButtonColumn(loka::app::ScrollViewNode &scrollView,
                                         int buttonCount)
  {
    loka::app::ColumnNode *column =
        new loka::app::ColumnNode((loka::app::ColumnProps()));
    for (int i = 0; i < buttonCount; ++i)
    {
      loka::app::ButtonProps props;
      column->addChild(new loka::app::ButtonNode(props));
    }
    scrollView.addChild(column);
    return column;
  }

  void establishLayout(Win32ScenePlatformController &controller,
                       loka::app::scene::Node *root,
                       int width,
                       int height)
  {
    controller.onChange(root,
                        loka::app::scene::NODE_DIRTY_NONE,
                        false);
    controller.relayout(width, height);
  }
} // namespace

void testWin32ScrollViewParentsAndClipsProjectedChildren()
{
  std::printf("\n==== [testWin32ScrollViewParentsAndClipsProjectedChildren] start ====\n");
  OffsetFact offset(10);
  loka::app::ScrollViewNode scrollView(
      (loka::app::ScrollViewProps(offset.state())));
  addButtonColumn(scrollView, 6);
  HWND root = createHostWindow();
  LOKA_VERIFY(root);
  {
    Win32ScenePlatformController controller(root);
    RegisterWin32BuiltInSupport(controller);
    establishLayout(controller, &scrollView, 300, 140);

    HWND viewport = findChildWindowByClass(root, L"LOKA_SCROLL_VIEW");
    LOKA_VERIFY(viewport);
    assert(GetParent(viewport) == root);
    const LONG_PTR viewportStyle = GetWindowLongPtr(viewport, GWL_STYLE);
    assert((viewportStyle & WS_VSCROLL) != 0);
    assert((viewportStyle & WS_CLIPCHILDREN) != 0);

    std::vector<HWND> buttons =
        directChildWindowsByClass(viewport, L"Button");
    assert(buttons.size() == 6);
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
      assert(GetParent(buttons[i]) == viewport &&
             "projected controls must be structurally clipped by the viewport HWND");
    }

    RECT viewportClient;
    LOKA_VERIFY(GetClientRect(viewport, &viewportClient));
    int maximumChildBottom = viewportClient.top;
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
      const RECT childRect = childRectInParent(buttons[i], viewport);
      if (childRect.bottom > maximumChildBottom)
      {
        maximumChildBottom = childRect.bottom;
      }
    }
    assert(maximumChildBottom > viewportClient.bottom &&
           "the tall subtree must extend beyond the viewport and rely on parent clipping");

    RECT first = childRectInParent(buttons[0], viewport);
    RECT second = childRectInParent(buttons[1], viewport);
    if (second.top < first.top)
    {
      const RECT swap = first;
      first = second;
      second = swap;
    }
    for (std::size_t i = 2; i < buttons.size(); ++i)
    {
      const RECT candidate = childRectInParent(buttons[i], viewport);
      if (candidate.top < first.top)
      {
        second = first;
        first = candidate;
      }
      else if (candidate.top < second.top)
      {
        second = candidate;
      }
    }
    assert(first.left == 0 && first.top == -10);
    assert(second.top == 34 &&
           "content y must project as viewport-client y minus offset");

    const SCROLLINFO info = scrollInfo(viewport);
    assert(info.nPage == 100);
    assert(maximumOffset(info) == 164);
    assert(info.nPos == 10);
  }
  LOKA_VERIFY(DestroyWindow(root));
  std::printf("==== [testWin32ScrollViewParentsAndClipsProjectedChildren] PASSED ====\n");
}

void testWin32ScrollViewOffsetIsRelayoutInput()
{
  std::printf("\n==== [testWin32ScrollViewOffsetIsRelayoutInput] start ====\n");
  OffsetFact offset(0);
  loka::app::ScrollViewNode scrollView(
      (loka::app::ScrollViewProps(offset.state())));
  addButtonColumn(scrollView, 6);
  HWND root = createHostWindow();
  LOKA_VERIFY(root);
  {
    Win32ScenePlatformController controller(root);
    RegisterWin32BuiltInSupport(controller);
    establishLayout(controller, &scrollView, 300, 140);
    HWND viewport = findChildWindowByClass(root, L"LOKA_SCROLL_VIEW");
    LOKA_VERIFY(viewport);
    std::vector<HWND> buttons = directChildWindowsByClass(viewport, L"Button");
    assert(!buttons.empty());
    const RECT atZero = childRectInParent(buttons[0], viewport);

    offset.state().set(7);
    controller.relayout(300, 140);
    const RECT afterChange = childRectInParent(buttons[0], viewport);
    const SCROLLINFO afterChangeInfo = scrollInfo(viewport);
    controller.relayout(300, 140);
    const RECT afterUnchangedRelayout = childRectInParent(buttons[0], viewport);

    assert(afterChange.top == atZero.top - 7);
    assert(afterChangeInfo.nPos == 7);
    assert(afterUnchangedRelayout.left == afterChange.left);
    assert(afterUnchangedRelayout.top == afterChange.top);
    assert(afterUnchangedRelayout.right == afterChange.right);
    assert(afterUnchangedRelayout.bottom == afterChange.bottom);
  }
  LOKA_VERIFY(DestroyWindow(root));
  std::printf("==== [testWin32ScrollViewOffsetIsRelayoutInput] PASSED ====\n");
}

void testWin32ScrollViewMessagePublishesOffsetFact()
{
  std::printf("\n==== [testWin32ScrollViewMessagePublishesOffsetFact] start ====\n");
  OffsetFact offset(0);
  loka::app::ScrollViewNode scrollView(
      (loka::app::ScrollViewProps(offset.state())));
  addButtonColumn(scrollView, 6);
  HWND root = createHostWindow();
  LOKA_VERIFY(root);
  {
    Win32ScenePlatformController controller(root);
    RegisterWin32BuiltInSupport(controller);
    establishLayout(controller, &scrollView, 300, 140);
    HWND viewport = findChildWindowByClass(root, L"LOKA_SCROLL_VIEW");
    LOKA_VERIFY(viewport);
    std::vector<HWND> buttons = directChildWindowsByClass(viewport, L"Button");
    assert(!buttons.empty());
    const RECT before = childRectInParent(buttons[0], viewport);

    SendMessageW(viewport, WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
    assert(offset.state().get() == 1 &&
           "WM_VSCROLL must publish through the ScrollView NodeState door");
    controller.relayout(300, 140);
    const RECT after = childRectInParent(buttons[0], viewport);
    assert(after.top == before.top - 1);

    SendMessageW(viewport, WM_VSCROLL, MAKEWPARAM(SB_THUMBTRACK, 10), 0);
    assert(offset.state().get() == 1 &&
           "thumb tracking is visual-only until the value settles");
    assert(scrollInfo(viewport).nPos == 10);
    SendMessageW(viewport, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, 10), 0);
    assert(offset.state().get() == 10 &&
           "thumb position must publish the settled fact");
    controller.relayout(300, 140);
    SendMessageW(viewport, WM_VSCROLL, MAKEWPARAM(SB_THUMBTRACK, 11), 0);
    assert(offset.state().get() == 10);
    SendMessageW(viewport, WM_VSCROLL, MAKEWPARAM(SB_ENDSCROLL, 0), 0);
    assert(offset.state().get() == 11 &&
           "end scroll must publish the last visual thumb position");
    controller.relayout(300, 140);

    const int rangeEnd = maximumOffset(scrollInfo(viewport));
    offset.state().set(rangeEnd);
    controller.relayout(300, 140);
    SendMessageW(viewport, WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
    assert(offset.state().get() == rangeEnd &&
           "line scrolling at the range end must clamp without another write");
  }
  LOKA_VERIFY(DestroyWindow(root));
  std::printf("==== [testWin32ScrollViewMessagePublishesOffsetFact] PASSED ====\n");
}

void testWin32ScrollViewResizeReclampsOffsetOnce()
{
  std::printf("\n==== [testWin32ScrollViewResizeReclampsOffsetOnce] start ====\n");
  OffsetFact offset(100);
  loka::app::ScrollViewNode scrollView(
      (loka::app::ScrollViewProps(offset.state())));
  addButtonColumn(scrollView, 6);
  HWND root = createHostWindow();
  LOKA_VERIFY(root);
  {
    Win32ScenePlatformController controller(root);
    RegisterWin32BuiltInSupport(controller);
    establishLayout(controller, &scrollView, 300, 100);
    HWND viewport = findChildWindowByClass(root, L"LOKA_SCROLL_VIEW");
    LOKA_VERIFY(viewport);
    assert(offset.state().get() == 100);

    offset.resetChangeCount();
    controller.relayout(300, 240);
    assert(offset.state().get() == 64);
    assert(offset.changeCount() == 1 &&
           "a range-changing resize must publish one clamped fact");
    assert(scrollInfo(viewport).nPos == 64);
    controller.relayout(300, 240);
    assert(offset.changeCount() == 1 &&
           "the settled clamp must not be published again");

    offset.state().set(20);
    offset.resetChangeCount();
    controller.relayout(300, 100);
    assert(offset.state().get() == 20);
    assert(offset.changeCount() == 0 &&
           "a still-valid offset must survive resize without a fact write");
  }
  LOKA_VERIFY(DestroyWindow(root));
  std::printf("==== [testWin32ScrollViewResizeReclampsOffsetOnce] PASSED ====\n");
}

void testWin32NestedScrollViewRefusesWithoutDisturbingOuterScope()
{
  std::printf("\n==== [testWin32NestedScrollViewRefusesWithoutDisturbingOuterScope] start ====\n");
  OffsetFact outerOffset(5);
  OffsetFact innerOffset(3);
  loka::app::ScrollViewNode outer(
      (loka::app::ScrollViewProps(outerOffset.state())));
  loka::app::ScrollViewNode *inner = new loka::app::ScrollViewNode(
      (loka::app::ScrollViewProps(innerOffset.state())));
  loka::app::ColumnNode *innerColumn = addButtonColumn(*inner, 1);
  loka::app::ButtonNode *innerButton =
      innerColumn->childrenHead()->asButtonNode();
  outer.addChild(inner);
  addButtonColumn(outer, 6);
  HWND root = createHostWindow();
  LOKA_VERIFY(root);
  {
    Win32ScenePlatformController controller(root);
    RegisterWin32BuiltInSupport(controller);
    establishLayout(controller, &outer, 300, 140);

    HWND viewport = findChildWindowByClass(root, L"LOKA_SCROLL_VIEW");
    LOKA_VERIFY(viewport);
    assert(findChildWindowByClass(viewport, L"LOKA_SCROLL_VIEW") == 0 &&
           "a nested ScrollView must refuse before materializing an HWND");
    LOKA_VERIFY(inner->getContext() == 0);
    LOKA_VERIFY(innerButton && innerButton->getContext() == 0);

    std::vector<HWND> outerButtons =
        directChildWindowsByClass(viewport, L"Button");
    assert(outerButtons.size() == 6);
    const RECT before = childRectInParent(outerButtons[0], viewport);
    SendMessageW(viewport, WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
    assert(outerOffset.state().get() == 6);
    assert(innerOffset.state().get() == 3);
    controller.relayout(300, 140);
    const RECT after = childRectInParent(outerButtons[0], viewport);
    assert(after.top == before.top - 1 &&
           "the outer scope must remain live after the inner refusal");
  }
  LOKA_VERIFY(DestroyWindow(root));
  std::printf("==== [testWin32NestedScrollViewRefusesWithoutDisturbingOuterScope] PASSED ====\n");
}
