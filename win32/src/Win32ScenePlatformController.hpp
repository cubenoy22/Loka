#ifndef LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP

#include <windows.h>
#include <vector>
#include "app/scene/projection/ProjectionParentScope.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/projection/PlatformLayoutHandler.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "platform/Win32DisplayFont.hpp"
#include "platform/Win32DisplayScale.hpp"

class Win32ButtonContext;
class Win32EditTextContext;
class Win32NativeLayoutPass;
class Win32PopupMenuContext;
class Win32RetirableContext;
class Win32ScrollViewContext;

namespace loka
{
  namespace core
  {
    namespace scene
    {
      class Node;
    }
  } // namespace core

  namespace app
  {
    class BoxNode;
    class RectSurfaceNode;
    class ScrollViewNode;
  } // namespace app

  namespace dsl
  {
    namespace testing
    {
      class Win32ScenePlatformTestAccess;
    }
  } // namespace dsl

  namespace app
  {
    namespace scene
    {
      class Win32PlatformLayoutTraversal;
    }
  } // namespace app
} // namespace loka

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

  struct LayoutState
  {
    typedef int Coordinate;

    int x;
    int y;
    int width;
    int height;
  };

  struct LayoutNodeResult
  {
    LayoutNodeResult()
        : boundaryWidth(0),
          resultY(0)
    {
    }
    LayoutNodeResult(int width, int y)
        : boundaryWidth(width),
          resultY(y)
    {
    }

    int boundaryWidth;
    int resultY;
  };

  Win32ScenePlatformController(HWND rootHwnd,
                               const loka::win32::Win32DisplayScale &displayScale);
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
  virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const
  {
    return true;
  }
  virtual void beginApplyCycle();
  virtual void synchronize();
  virtual bool hasPendingSync() const;
  virtual void drainNativeRetirements();
  virtual void destroy();
  virtual void releaseNodeContexts(loka::app::scene::Node *node);
  virtual bool prepareProjectedLayout(loka::app::scene::Node *node, loka::app::scene::LayoutState &state);
  virtual bool registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler);

  bool handleCommand(WPARAM wParam, LPARAM lParam);
  /** Relayout entry for WM_SIZE and other device-pixel client readings. */
  void relayoutNativeClientPixels(int clientWidth, int clientHeight);
  void relayout(int clientWidth, int clientHeight);
  /** Replaces the projection fact and, when allocation succeeds, the native
      subtree's DPI-derived font without exposing an unowned font handle. */
  void updateDisplayScale(const loka::win32::Win32DisplayScale &displayScale);
  const loka::win32::Win32DisplayScale &displayScale() const
  {
    return this->displayScale_;
  }
  HFONT displayFont() const
  {
    return this->displayFont_.get();
  }
  HWND rootHwnd() const
  {
    return rootHwnd_;
  }
  /** Native parent for a projected child. The root remains the default;
      an active ScrollView scope supplies its viewport HWND. */
  HWND projectionParentHwnd() const
  {
    return static_cast<HWND>(this->projectionParentScopes_.current().nativeParent);
  }
  void queueNativeRetirement(HWND hwnd);

private:
  friend class ::loka::dsl::testing::Win32ScenePlatformTestAccess;
  friend class ::loka::app::scene::Win32PlatformLayoutTraversal;
  friend class ::Win32NativeLayoutPass;
  friend class ::Win32RetirableContext;
  friend void RegisterWin32BuiltInSupport(Win32ScenePlatformController &controller);

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
    PendingInvalidate()
        : hwnd(0),
          eraseBackground(TRUE),
          fullWindow(false),
          includeChildren(false)
    {
      rect.left = rect.top = rect.right = rect.bottom = 0;
    }
    HWND hwnd;
    RECT rect;
    BOOL eraseBackground;
    bool fullWindow;
    bool includeChildren;
  };

  typedef LayoutNodeResult (*LeafLayoutHandlerFn)(Win32ScenePlatformController *,
                                                  loka::app::scene::Node *,
                                                  const LayoutState &);
  static LayoutNodeResult DispatchProjectedLayout(
      Win32ScenePlatformController *controller,
      loka::app::scene::Node *node,
      const LayoutState &state);

  struct LeafLayoutHandlerEntry
  {
    LeafLayoutHandlerEntry(const void *nodeTypeKey, LeafLayoutHandlerFn handler)
        : nodeTypeKey_(nodeTypeKey),
          handler_(handler),
          next_(0)
    {
    }

    const void *nodeTypeKey_;
    LeafLayoutHandlerFn handler_;
    LeafLayoutHandlerEntry *next_;
  };

  class LeafLayoutHandlerRegistry
  {
  public:
    LeafLayoutHandlerRegistry()
        : head_(0)
    {
    }
    ~LeafLayoutHandlerRegistry()
    {
      LeafLayoutHandlerEntry *entry = this->head_;
      while (entry)
      {
        LeafLayoutHandlerEntry *next = entry->next_;
        delete entry;
        entry = next;
      }
      this->head_ = 0;
    }

    bool registerHandler(const void *nodeTypeKey, LeafLayoutHandlerFn handler)
    {
      if (!nodeTypeKey || !handler)
      {
        return false;
      }
      LeafLayoutHandlerEntry *existing = this->head_;
      while (existing)
      {
        if (existing->nodeTypeKey_ == nodeTypeKey)
        {
          existing->handler_ = handler;
          return true;
        }
        existing = existing->next_;
      }
      LeafLayoutHandlerEntry *entry = new LeafLayoutHandlerEntry(nodeTypeKey, handler);
      if (!entry)
      {
        return false;
      }
      entry->next_ = this->head_;
      this->head_ = entry;
      return true;
    }

    LeafLayoutHandlerFn find(const loka::app::scene::Node *node) const
    {
      if (!node)
      {
        return 0;
      }
      const void *nodeTypeKey = node->nodeTypeKey();
      if (!nodeTypeKey)
      {
        return 0;
      }
      LeafLayoutHandlerEntry *entry = this->head_;
      while (entry)
      {
        if (entry->nodeTypeKey_ == nodeTypeKey)
        {
          return entry->handler_;
        }
        entry = entry->next_;
      }
      return 0;
    }

  private:
    LeafLayoutHandlerEntry *head_;

    LeafLayoutHandlerRegistry(const LeafLayoutHandlerRegistry &);
    LeafLayoutHandlerRegistry &operator=(const LeafLayoutHandlerRegistry &);
  };

  static int layoutContainerChild(void *context, loka::app::scene::Node *child, const LayoutState &state);
  int layoutNodeFromSceneState(loka::app::scene::Node *node, const loka::app::scene::LayoutState &state);
  int layoutNode(loka::app::scene::Node *node, const LayoutState &state);
  LayoutNodeResult computeLayoutResult(loka::app::scene::Node *node, const LayoutState &state);
  LayoutNodeResult layoutScrollViewNode(loka::app::ScrollViewNode *scrollView, const LayoutState &state);
  int applyBoundaryLayoutResult(loka::app::scene::BoundaryNode *boundary, int x, int y, const LayoutNodeResult &result);
  LayoutNodeResult layoutRectSurfaceNode(loka::app::RectSurfaceNode *surface, const LayoutState &state);
  bool narrowLayoutState(const LayoutState &state,
                         loka::app::scene::LayoutState &narrowed,
                         bool applyProjection);
  bool refuseNarrowingInScrollScope(int resultY);
  void refuseScrollViewShortRange();
  void performLayout(int clientWidth, int clientHeight);
  /** Position one projected HWND. A root layout pass suppresses per-child
      repaint and presents the completed native layout once at the pass end. */
  void positionNativeWindow(HWND hwnd, int x, int y, int width, int height);
  HWND createNativeChildWindow(DWORD exStyle,
                               LPCWSTR className,
                               LPCWSTR windowName,
                               DWORD style,
                               int x,
                               int y,
                               int width,
                               int height,
                               HWND parent,
                               HMENU menu,
                               HINSTANCE instance,
                               void *createParameter);
  void applyDisplayFontToNativeSubtree(HFONT font);
  void clearContexts();
  void clearNodeContexts(loka::app::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;
  void queueDirtyRect(HWND targetHwnd, const RECT *rect, BOOL eraseBackground, bool includeChildren);
  void dumpRedrawStatsIfNeeded();

  HWND rootHwnd_;
  Win32NativeLayoutPass *activeNativeLayoutPass_;
  loka::app::scene::ProjectionParentScopeStack projectionParentScopes_;
  loka::app::scene::PlatformLayoutHandlerRegistry layoutHandlerRegistry_;
  loka::app::scene::PlatformNodeHandlerRegistry nodeHandlerRegistry_;
  LeafLayoutHandlerRegistry leafLayoutHandlerRegistry_;
  LeafLayoutHandlerRegistry hostActionHandlerRegistry_;
  loka::app::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  loka::win32::Win32DisplayScale displayScale_;
  loka::win32::Win32DisplayFont displayFont_;
  std::vector<PendingInvalidate> pendingInvalidations_;
  std::vector<HWND> retiredWindows_;
  RedrawStats redrawStats_;
};

#endif // LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
