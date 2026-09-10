#ifndef SMIRKYCARD_APP_CONFIG_HPP
#define SMIRKYCARD_APP_CONFIG_HPP

#include "CardNodes.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/Menu.hpp"

/** RunApp destroys the App and its windows before this configuration. */
class SmirkyCardAppConfig : public AppConfigurable
{
public:
  explicit SmirkyCardAppConfig(PlatformContext *context)
      : AppConfigurable(context),
        runtime_()
  {
  }

  virtual void compose(AppComposition &composition)
  {
    composition << WindowDef(WindowProps()
                                 .frame(60, 60, 420, 240)
                                 .title("SmirkyCard")
                                 .visible(true)
                                 .scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, this->runtime_)));
  }

  virtual void composeMenu(loka::app::MenuComposition &composition)
  {
    using namespace loka::app;
    composition.declare(AppMenu() << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
  }

private:
  smirkycard::ScriptRuntime runtime_;
};
#endif
