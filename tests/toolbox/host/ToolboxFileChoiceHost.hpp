#ifndef LOKA_TEST_TOOLBOX_FILE_CHOICE_HOST_HPP
#define LOKA_TEST_TOOLBOX_FILE_CHOICE_HOST_HPP
// Replace only the unrelated controller/cursor neighbor. Compile the complete
// production dialog TU and its real NodeState result delivery against common/.
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#include "app/scene/projection/PlatformController.hpp"
class CursorOwner { public: void reconcile() {} };
class ToolboxScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  CursorOwner *cursorOwner() { return 0; }
};
#endif
