#ifndef LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP

#include "app/scene/PlatformController.hpp"
#include "app/scene/PlatformLayoutHandler.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include "context/MacNodeContextMapper.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class MacScenePlatformTestAccess;
    }
  }
}

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
    class ButtonNode;
    class CellNode;
    class EditTextNode;
    class ImageViewNode;
    class OpenFileDialogNode;
    class PopupMenuNode;
    class RectSurfaceNode;
    class TextNode;

    namespace scene
    {
      class MacPlatformLayoutTraversal;
    }
  }
}

class MacScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  explicit MacScenePlatformController(void *rootView);
  virtual ~MacScenePlatformController();

  virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild);
  virtual void onBoundaryApply(loka::app::scene::Node *rootNode,
                               loka::app::scene::BoundaryNode *boundary,
                               const loka::app::scene::BoundaryLocalApplyInfo &info,
                               const loka::app::scene::PlatformApplyPlan &plan);
  virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const { return true; }
  virtual void synchronize();
  virtual bool hasPendingSync() const;
  virtual void destroy();
  virtual void releaseNodeContexts(loka::app::scene::Node *node);
  virtual bool registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler);

  void relayout(int clientWidth, int clientHeight);
  void requestRelayout();
  bool hasPendingRelayout() const { return relayoutPending_; }
  static MacScenePlatformController *findForRootView(void *rootView);
  static void flushPendingRelayouts();
  MacNodeContextMapper *contextMapper() { return &contextMapper_; }

private:
  friend class ::loka::dsl::testing::MacScenePlatformTestAccess;
  friend class ::loka::app::scene::MacPlatformLayoutTraversal;

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

  static int layoutContainerChild(void *context, loka::app::scene::Node *child, const LayoutState &state);
  int layoutNodeFromSceneState(loka::app::scene::Node *node, const loka::app::scene::LayoutState &state);
  int layoutNode(loka::app::scene::Node *node, const LayoutState &state);
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
  void registerEditField(void *field);
  void finalizeKeyLoop();
  void captureFocusedEditField();
  void restoreFocusedEditField();
  void *findFocusedEditTextState(loka::app::scene::Node *node) const;
  void *findFieldForFocusedEdit(loka::app::scene::Node *node) const;

  void *rootView_;
  MacNodeContextMapper contextMapper_;
  loka::app::scene::PlatformLayoutHandlerRegistry layoutHandlerRegistry_;
  loka::app::scene::PlatformNodeHandlerRegistry nodeHandlerRegistry_;
  loka::app::scene::Node *rootNode_;
  loka::app::scene::NodeDirtyFlags lastChangeFlags_;
  int clientWidth_;
  int clientHeight_;
  void *firstEditField_;
  void *lastEditField_;
  void *focusedEditTextState_;
  int focusedEditTextControlTag_;
  bool relayoutPending_;
};

#endif // LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
