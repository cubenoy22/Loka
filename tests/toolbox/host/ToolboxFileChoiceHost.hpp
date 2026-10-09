#ifndef LOKA_TEST_TOOLBOX_FILE_CHOICE_HOST_HPP
#define LOKA_TEST_TOOLBOX_FILE_CHOICE_HOST_HPP
// Native neighbors only: production dialog, input door and present are tested.
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#include "app/scene/projection/PlatformController.hpp"
#include "Quickdraw.h"
#include "ToolboxPendingDialogs.hpp"
struct Point { short v, h; };
class CursorOwner
{
public:
  unsigned reconciles;
  CursorOwner() : reconciles(0) {}
  void reconcile() { ++this->reconciles; }
};
class ToolboxScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  ToolboxPendingDialogs pending_;
  ToolboxPendingDialogs &pendingDialogs() { return this->pending_; }
  void (*onRender)(void *);
  void *renderData;
  ToolboxScenePlatformController() : onRender(0), renderData(0) {}
  CursorOwner *cursorOwner() { return 0; }
  void render() { if (this->onRender) this->onRender(this->renderData); }
  void renderDirty(const Rect &) { this->render(); }
  bool handleMouseDown(const Point &) { return false; }
  bool handleKeyDown(char) { return false; }
  void idleTextEdits() {}
  void onChange(loka::app::scene::Node *, loka::app::scene::NodeDirtyFlags, bool) {}
  void synchronize() {}
  bool hasPendingSync() const { return false; }
  void destroy() {}
};
#endif
