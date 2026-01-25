#ifndef LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP

#include "core2/scene/PlatformController.hpp"

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

class MacScenePlatformController : public loka::core::scene::IPlatformController
{
public:
  explicit MacScenePlatformController(void *rootView);
  virtual ~MacScenePlatformController();

  virtual void onChange(loka::core::scene::Node *rootNode, loka::core::scene::NodeDirtyFlags flags);
  virtual void synchronize();
  virtual void destroy();

  void relayout(int clientWidth, int clientHeight);

private:
  struct LayoutState
  {
    int x;
    int y;
    int width;
    int height;
  };

  int layoutNode(loka::core::scene::Node *node, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight);
  void clearContexts();
  void clearNodeContexts(loka::core::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;
  void registerEditField(void *field);
  void finalizeKeyLoop();

  void *rootView_;
  loka::core::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  void *firstEditField_;
  void *lastEditField_;
};

#endif // LOKA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
