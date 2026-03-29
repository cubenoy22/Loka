#ifndef LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP

#include <windows.h>
#include <map>
#include "app/scene/PlatformController.hpp"
#include "app/scene/PlatformLayoutHandler.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include "context/Win32NodeContextMapper.hpp"

class Win32ButtonContext;
class Win32EditTextContext;
class Win32PopupMenuContext;

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
    class CellNode;
    class TextNode;
    class EditTextNode;
    class ImageViewNode;
    class OpenFileDialogNode;
    class PopupMenuNode;
    class RectSurfaceNode;
  }

  namespace dsl
  {
    namespace testing
    {
      class Win32ScenePlatformTestAccess;
    }
  }

  namespace app
  {
    namespace scene
    {
      class Win32PlatformLayoutTraversal;
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
  virtual bool prepareProjectedLayout(loka::app::scene::Node *node, loka::app::scene::LayoutState &state);
  virtual bool registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler);

  bool handleCommand(WPARAM wParam, LPARAM lParam);
  void relayout(int clientWidth, int clientHeight);
  Win32NodeContextMapper *contextMapper() { return &contextMapper_; }

private:
  friend class ::loka::dsl::testing::Win32ScenePlatformTestAccess;
  friend class ::loka::app::scene::Win32PlatformLayoutTraversal;
  friend void RegisterWin32PlatformLeafLayoutHandlers(Win32ScenePlatformController &controller);
  friend void RegisterWin32PlatformHostActionLayoutHandlers(Win32ScenePlatformController &controller);

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

  struct LayoutNodeResult
  {
    LayoutNodeResult() : boundaryWidth(0), resultY(0) {}
    LayoutNodeResult(int width, int y) : boundaryWidth(width), resultY(y) {}

    int boundaryWidth;
    int resultY;
  };

  typedef LayoutNodeResult (*LeafLayoutHandlerFn)(Win32ScenePlatformController *,
                                                  loka::app::scene::Node *,
                                                  const LayoutState &);

  struct LeafLayoutHandlerEntry
  {
    LeafLayoutHandlerEntry(const void *nodeTypeKey, LeafLayoutHandlerFn handler)
        : nodeTypeKey_(nodeTypeKey), handler_(handler), next_(0)
    {
    }

    const void *nodeTypeKey_;
    LeafLayoutHandlerFn handler_;
    LeafLayoutHandlerEntry *next_;
  };

  class LeafLayoutHandlerRegistry
  {
  public:
    LeafLayoutHandlerRegistry() : head_(0) {}
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
  static LayoutNodeResult dispatchTextLayout(Win32ScenePlatformController *controller,
                                             loka::app::scene::Node *node,
                                             const LayoutState &state);
  static LayoutNodeResult dispatchImageViewLayout(Win32ScenePlatformController *controller,
                                                  loka::app::scene::Node *node,
                                                  const LayoutState &state);
  static LayoutNodeResult dispatchButtonLayout(Win32ScenePlatformController *controller,
                                               loka::app::scene::Node *node,
                                               const LayoutState &state);
  static LayoutNodeResult dispatchEditTextLayout(Win32ScenePlatformController *controller,
                                                 loka::app::scene::Node *node,
                                                 const LayoutState &state);
  static LayoutNodeResult dispatchPopupMenuLayout(Win32ScenePlatformController *controller,
                                                  loka::app::scene::Node *node,
                                                  const LayoutState &state);
  static LayoutNodeResult dispatchCellLayout(Win32ScenePlatformController *controller,
                                             loka::app::scene::Node *node,
                                             const LayoutState &state);
  static LayoutNodeResult dispatchOpenFileDialogLayout(Win32ScenePlatformController *controller,
                                                       loka::app::scene::Node *node,
                                                       const LayoutState &state);
  int layoutNodeFromSceneState(loka::app::scene::Node *node, const loka::app::scene::LayoutState &state);
  int layoutNode(loka::app::scene::Node *node, const LayoutState &state);
  LayoutNodeResult computeLayoutResult(loka::app::scene::Node *node, const LayoutState &state);
  int applyBoundaryLayoutResult(loka::app::scene::BoundaryNode *boundary,
                                int x,
                                int y,
                                const LayoutNodeResult &result);
  LayoutNodeResult layoutOpenFileDialogNode(loka::app::OpenFileDialogNode *dialog, const LayoutState &state);
  LayoutNodeResult layoutButtonNode(loka::app::ButtonNode *button, const LayoutState &state);
  LayoutNodeResult layoutEditTextNode(loka::app::EditTextNode *edit, const LayoutState &state);
  LayoutNodeResult layoutPopupMenuNode(loka::app::PopupMenuNode *popup, const LayoutState &state);
  LayoutNodeResult layoutCellNode(loka::app::CellNode *cell, const LayoutState &state);
  LayoutNodeResult layoutTextNode(loka::app::TextNode *text, const LayoutState &state);
  LayoutNodeResult layoutImageViewNode(loka::app::ImageViewNode *image, const LayoutState &state);
  LayoutNodeResult layoutRectSurfaceNode(loka::app::RectSurfaceNode *surface, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight, bool rebuildContexts);
  void clearContexts();
  void clearNodeContexts(loka::app::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;
  void queueDirtyRect(HWND targetHwnd, const RECT *rect, BOOL eraseBackground, bool includeChildren);
  void dumpRedrawStatsIfNeeded();

  HWND rootHwnd_;
  Win32NodeContextMapper contextMapper_;
  loka::app::scene::PlatformLayoutHandlerRegistry layoutHandlerRegistry_;
  loka::app::scene::PlatformNodeHandlerRegistry nodeHandlerRegistry_;
  LeafLayoutHandlerRegistry leafLayoutHandlerRegistry_;
  LeafLayoutHandlerRegistry hostActionHandlerRegistry_;
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
