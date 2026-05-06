#ifndef LOKA_TUTORIAL_APP_CONFIG_HPP
#define LOKA_TUTORIAL_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/Menu.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/scene/composition/StdComposition.hpp"
#include "DoItYourselfNode.hpp"
#include "Step1Node.hpp"
#include "Step2Node.hpp"
#include "Step3Node.hpp"
#include "Step4Node.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx)
  {
  }

  virtual void compose(AppComposition &c)
  {
    typedef tutorial::DoItYourselfNode TutorialNode;
    // typedef tutorial::Step1Node TutorialNode;
    // typedef tutorial::Step2Node TutorialNode;
    // typedef tutorial::Step3Node TutorialNode;
    // typedef tutorial::Step4Node TutorialNode;

    c << WindowDef( //
        WindowProps()
            .frame(60, 60, 360, 280)
            .scene(loka::app::scene::Boundary<TutorialNode>())
            .title("LokaTutorial")
            .visible(true));
  }

  virtual void composeMenu(loka::app::MenuComposition &c)
  {
    using namespace loka::app;
    c.declare(AppMenu()                                 //
              << MenuItem("About")                      //
                     .actionType(MENU_ACTION_ABOUT_APP) //
              << MenuSeparator()                        //
              << MenuItem("Quit")                       //
                     .actionType(MENU_ACTION_QUIT_APP));
  }
};

#endif // LOKA_TUTORIAL_APP_CONFIG_HPP
