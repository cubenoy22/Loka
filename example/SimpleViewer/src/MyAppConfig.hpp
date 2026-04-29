#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "loka/core/State.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/Menu.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable {
public:
  explicit MyAppConfig(PlatformContext *ctx) : AppConfigurable(ctx), openDialogEvent_() {
  }

  virtual void compose(AppComposition &c) {
    c << WindowDef(
        WindowProps()
            .frame(40, 40, 320, 240)
            .scene(loka::app::scene::NodeDefinition<simpleviewer::MainProps, simpleviewer::MainNode>(
                simpleviewer::MainProps()
                    .platformContext(this->getPlatformContext()) // TODO: Make this retrievable from inside the Node
                    .openDialogEvent(&this->openDialogEvent_)))
            .title("LokaSimpleViewer")
            .visible(true));
  }

  virtual void composeMenu(loka::app::MenuComposition &c) {
    using namespace loka::app;
    c.declare(                                                 //
        AppMenu()                                              //
        << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP) //
        << MenuSeparator()                                     //
        << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP)   //
    );
    c.declare(       //
        Menu("File") //
        << MenuItem("Open...").onClick(&this->openDialogEvent_));
  }

private:
  loka::core::EmitterState openDialogEvent_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
