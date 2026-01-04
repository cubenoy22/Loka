#ifndef DECLARA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
#define DECLARA_MAC_SCENE_PLATFORM_CONTROLLER_HPP

#include "core2/scene/PlatformController.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class Node;
    }
  }
}

class MacScenePlatformController : public declara::core::scene::IPlatformController
{
public:
  explicit MacScenePlatformController(void *rootView);
  virtual ~MacScenePlatformController();

  virtual void onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags flags);
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

  int layoutNode(declara::core::scene::Node *node, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight);
  void clearContexts();
  void clearNodeContexts(declara::core::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;
  void registerEditField(void *field);
  void finalizeKeyLoop();

  void *rootView_;
  declara::core::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  void *firstEditField_;
  void *lastEditField_;
};

#endif // DECLARA_MAC_SCENE_PLATFORM_CONTROLLER_HPP
