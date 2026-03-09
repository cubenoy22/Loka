#ifndef LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP

#include "app/scene/PlatformController.hpp"

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

  virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags);
  virtual void synchronize();
  virtual void destroy();

  void relayout(int clientWidth, int clientHeight);
  void requestRelayout();
  bool hasPendingRelayout() const { return relayoutPending_; }
  static MacScenePlatformController *findForRootView(void *rootView);
  static void flushPendingRelayouts();

private:
  struct LayoutState
  {
    int x;
    int y;
    int width;
    int height;
  };

  int layoutNode(loka::app::scene::Node *node, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight);
  void clearContexts();
  void clearNodeContexts(loka::app::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;
  void registerEditField(void *field);
  void finalizeKeyLoop();

  void *rootView_;
  loka::app::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  void *firstEditField_;
  void *lastEditField_;
  bool relayoutPending_;
};

#endif // LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
