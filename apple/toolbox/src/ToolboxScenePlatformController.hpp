#ifndef LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP

#include "core2/scene/PlatformController.hpp"

class ToolboxWindow;

class ToolboxScenePlatformController : public declara::core::scene::IPlatformController
{
public:
  explicit ToolboxScenePlatformController(ToolboxWindow *window);
  virtual ~ToolboxScenePlatformController();

  virtual void onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags flags);
  virtual void synchronize();
  virtual void destroy();

  void render();

private:
  ToolboxWindow *window_;
  declara::core::scene::Node *rootNode_;
};

#endif // LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
