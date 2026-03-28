#ifndef LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP

#include <windows.h>
#include <map>
#include "app/scene/PlatformController.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include "context/Win32ButtonContext.hpp"
#include "context/Win32TextContext.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32NodeContextMapper.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include "context/Win32PopupMenuContext.hpp"

namespace loka
{
  namespace core
  {
    namespace scene
    {
      class Node;
    }
  }

  namespace app
  {
    class BoxNode;
    class ButtonNode;
    class TextNode;
    class EditTextNode;
  }

  namespace dsl
  {
    namespace testing
    {
      class Win32ScenePlatformTestAccess;
    }
  }
}

class Win32ScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  enum NativePaintKind
  {
    NATIVE_PAINT_ROOT = 0,
    NATIVE_PAINT_CELL = 1,
    NATIVE_PAINT_IMAGE = 2,
    NATIVE_PAINT_RECT_SURFACE = 3
  };

  explicit Win32ScenePlatformController(HWND rootHwnd);
  virtual ~Win32ScenePlatformController();

  static void requestDirtyRect(HWND targetHwnd, const RECT *rect, BOOL eraseBackground);
  static void requestDirtySubtree(HWND targetHwnd, const RECT *rect, BOOL eraseBackground);
  static void redrawDirtySubtreeNow(HWND targetHwnd, const RECT *rect, BOOL eraseBackground);
  static void noteNativePaint(HWND targetHwnd, NativePaintKind kind, bool eraseBackground);

  virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild);
  virtual void onBoundaryApply(loka::app::scene::Node *rootNode,
                               loka::app::scene::BoundaryNode *boundary,
                               const loka::app::scene::BoundaryLocalApplyInfo &info,
                               const loka::app::scene::PlatformApplyPlan &plan);
  virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const { return true; }
  virtual void beginApplyCycle();
  virtual void synchronize();
  virtual bool hasPendingSync() const;
  virtual void destroy();
  virtual void releaseNodeContexts(loka::app::scene::Node *node);

  bool handleCommand(WPARAM wParam, LPARAM lParam);
  void relayout(int clientWidth, int clientHeight);
  Win32NodeContextMapper *contextMapper() { return &contextMapper_; }

private:
  friend class ::loka::dsl::testing::Win32ScenePlatformTestAccess;

  struct RedrawStats
  {
    RedrawStats()
        : onChangeCalls(0),
          onBoundaryApplyCalls(0),
          lastOnChangeFlags(loka::app::scene::NODE_DIRTY_NONE),
          lastOnChangeRequiredLayout(false),
          lastOnChangeFullRebuild(false),
          queuedFullWindowInvalidates(0),
          queuedRectInvalidates(0),
          queuedLayoutBoundsInvalidates(0),
          queuedPaintBoundsInvalidates(0),
          queuedMissingBoundsInvalidates(0),
          queuedCompositedInvalidates(0),
          queuedOpaquePaintInvalidates(0),
          queuedGenericPaintInvalidates(0),
          rootEraseCount(0),
          rootPaintCount(0),
          cellEraseCount(0),
          cellPaintCount(0),
          imageEraseCount(0),
          imagePaintCount(0),
          rectSurfaceEraseCount(0),
          rectSurfacePaintCount(0)
    {
    }

    void reset()
    {
      onChangeCalls = 0;
      onBoundaryApplyCalls = 0;
      lastOnChangeFlags = loka::app::scene::NODE_DIRTY_NONE;
      lastOnChangeRequiredLayout = false;
      lastOnChangeFullRebuild = false;
      queuedFullWindowInvalidates = 0;
      queuedRectInvalidates = 0;
      queuedLayoutBoundsInvalidates = 0;
      queuedPaintBoundsInvalidates = 0;
      queuedMissingBoundsInvalidates = 0;
      queuedCompositedInvalidates = 0;
      queuedOpaquePaintInvalidates = 0;
      queuedGenericPaintInvalidates = 0;
      rootEraseCount = 0;
      rootPaintCount = 0;
      cellEraseCount = 0;
      cellPaintCount = 0;
      imageEraseCount = 0;
      imagePaintCount = 0;
      rectSurfaceEraseCount = 0;
      rectSurfacePaintCount = 0;
    }

    int onChangeCalls;
    int onBoundaryApplyCalls;
    loka::app::scene::NodeDirtyFlags lastOnChangeFlags;
    bool lastOnChangeRequiredLayout;
    bool lastOnChangeFullRebuild;
    int queuedFullWindowInvalidates;
    int queuedRectInvalidates;
    int queuedLayoutBoundsInvalidates;
    int queuedPaintBoundsInvalidates;
    int queuedMissingBoundsInvalidates;
    int queuedCompositedInvalidates;
    int queuedOpaquePaintInvalidates;
    int queuedGenericPaintInvalidates;
    int rootEraseCount;
    int rootPaintCount;
    int cellEraseCount;
    int cellPaintCount;
    int imageEraseCount;
    int imagePaintCount;
    int rectSurfaceEraseCount;
    int rectSurfacePaintCount;
  };

  struct PendingInvalidate
  {
    PendingInvalidate() : hwnd(0), eraseBackground(TRUE), fullWindow(false), includeChildren(false)
    {
      rect.left = rect.top = rect.right = rect.bottom = 0;
    }
    HWND hwnd;
    RECT rect;
    BOOL eraseBackground;
    bool fullWindow;
    bool includeChildren;
  };

  struct LayoutState
  {
    int x;
    int y;
    int width;
    int height;
  };

  int layoutNode(loka::app::scene::Node *node, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight, bool rebuildContexts);
  void clearContexts();
  void clearNodeContexts(loka::app::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;
  void queueDirtyRect(HWND targetHwnd, const RECT *rect, BOOL eraseBackground, bool includeChildren);
  void dumpRedrawStatsIfNeeded();

  HWND rootHwnd_;
  Win32NodeContextMapper contextMapper_;
  loka::app::scene::PlatformNodeHandlerRegistry nodeHandlerRegistry_;
  loka::app::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  std::map<HWND, Win32ButtonContext *> buttonMap_;
  std::map<HWND, Win32EditTextContext *> editMap_;
  std::map<HWND, Win32PopupMenuContext *> popupMap_;
  std::vector<PendingInvalidate> pendingInvalidations_;
  RedrawStats redrawStats_;
};

#endif // LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
