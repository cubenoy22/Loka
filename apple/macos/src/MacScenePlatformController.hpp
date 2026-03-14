#ifndef LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP

#include "app/scene/PlatformController.hpp"

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
}

class MacScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  explicit MacScenePlatformController(void *rootView);
  virtual ~MacScenePlatformController();

  virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild);
  virtual void synchronize();
  virtual bool hasPendingSync() const;
  virtual void destroy();

  void relayout(int clientWidth, int clientHeight);
  void requestRelayout();
  bool hasPendingRelayout() const { return relayoutPending_; }
  static MacScenePlatformController *findForRootView(void *rootView);
  static void flushPendingRelayouts();

private:
  friend class ::loka::dsl::testing::MacScenePlatformTestAccess;

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
  void registerEditField(void *field);
  void finalizeKeyLoop();
  void captureFocusedEditField();
  void restoreFocusedEditField();
  void *findFocusedEditTextState(loka::app::scene::Node *node) const;
  void *findFieldForFocusedEdit(loka::app::scene::Node *node) const;

  void *rootView_;
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
