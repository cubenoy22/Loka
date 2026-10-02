#ifndef LOKA_TEST_TOOLBOX_MENU_HOST_HPP
#define LOKA_TEST_TOOLBOX_MENU_HOST_HPP
/** Substitute App/window/controller neighbors while compiling the production
    attachment, App forwarding bodies and controller releaseMenu body unchanged. */
#define LOKA_TOOLBOX_APP_HPP
#define LOKA_TOOLBOX_WINDOW_HPP
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#include "app/core/App.hpp"
#include "app/core/Window.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "ToolboxMenuAttachment.hpp"

class ToolboxApp : public App
{
public:
  explicit ToolboxApp(AppConfigurable *config = 0) : App(config), menuAttachment_(*this), redraws(0), quits(0) {}
  virtual ~ToolboxApp();
  virtual void applyMenuBar(Window *window);
  void handleMenuSelection(short menuId, short item);
  ToolboxMenuAttachment &menuAttachment() { return this->menuAttachment_; }
  void noteMenuBarChangedFromBinding() { ++this->redraws; }
  virtual void quit() { ++this->quits; }
private:
  ToolboxMenuAttachment menuAttachment_;
public:
  int redraws, quits;
};
class ToolboxWindow : public Window
{
public:
  ToolboxWindow(PlatformContext *platform, const WindowProps &props, ToolboxApp *app)
      : Window(platform, props), app_(app), positionPreserves(0) {}
  ToolboxApp *toolboxApp() const { return this->app_; }
  virtual ToolboxWindow *asToolboxWindow() { return this; }
  void preserveNativeContentPositionAfterMenuBarChange() { ++this->positionPreserves; }
  void unmount() { this->unmountSceneForTeardown(*this->scene()); }
private:
  ToolboxApp *app_;
public:
  int positionPreserves;
};
class ToolboxScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  explicit ToolboxScenePlatformController(ToolboxWindow *window) : window_(window) {}
  virtual void releaseMenu();
  virtual void onChange(loka::app::scene::Node *, loka::app::scene::NodeDirtyFlags, bool) {}
  virtual void synchronize() {}
  virtual bool hasPendingSync() const { return false; }
  virtual void destroy() {}
private:
  ToolboxWindow *window_;
};
#endif
