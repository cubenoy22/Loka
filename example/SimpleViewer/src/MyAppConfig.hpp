#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "core/AppComposition.hpp"
#include "core/AppConfigurable.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/WindowDefinition.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/Menu.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx),
        openDialogVisible_(false),
        tracker_(),
        openDialogEvent_()
  {
    this->tracker_.addState(&this->openDialogVisible_);
    this->openDialogEvent_.deferBind(&MyAppConfig::OpenDialogThunk, this);
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(40, 40, 320, 240)
                       .scene(declara::core::scene::NodeDefinition<simpleviewer::MainProps, simpleviewer::MainNode>(
                           simpleviewer::MainProps().dialogVisible(&this->openDialogVisible_)))
                       .title("LokaSimpleViewer")
                       .visible(true));
  }

  virtual void composeMenu(declara::app::MenuComposition &c)
  {
    using namespace declara::app;
    c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
                        << MenuSeparator()
                        << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
    c.declare(Menu("File") << MenuItem("Open...").onClick(&this->openDialogEvent_));
  }

private:
  void handleOpenDialog()
  {
    StateTrackerGuard guard(&this->tracker_);
    if (this->openDialogVisible_.get())
    {
      this->openDialogVisible_.set(false, true);
    }
    this->openDialogVisible_.set(true, true);
  }

  static void OpenDialogThunk(void *userData)
  {
    MyAppConfig *self = static_cast<MyAppConfig *>(userData);
    if (self)
    {
      self->handleOpenDialog();
    }
  }

  declara::core::MutableState<bool> openDialogVisible_;
  declara::core::PushStateTracker tracker_;
  declara::core::EmitterState openDialogEvent_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
