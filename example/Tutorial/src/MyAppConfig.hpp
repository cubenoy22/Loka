#ifndef LOKA_TUTORIAL_APP_CONFIG_HPP
#define LOKA_TUTORIAL_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/Menu.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
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

    loka::app::scene::BoundaryDefinition<TutorialNode::PropsType, TutorialNode> mainDefinition;
    c << WindowDef(this->productionWindowProps(mainDefinition));
  }

  virtual void composeMenu(loka::app::MenuComposition &c)
  {
    tutorial::DeclareTutorialMenu(c);
  }

protected:
  /** Declares Tutorial's production window presentation around a supplied
      scene so non-production vehicles cannot drift its title or frame. */
  WindowProps productionWindowProps(const loka::app::scene::NodeDefinitionBase &scene) const
  {
    return WindowProps()
        .frame(60, 60, 360, 280)
        .scene(scene)
        .title("LokaTutorial")
        .visible(true);
  }
};

#endif // LOKA_TUTORIAL_APP_CONFIG_HPP
