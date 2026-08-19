#ifndef LOKA_SCRAPBOOK_UI_APP_CONFIG_HPP
#define LOKA_SCRAPBOOK_UI_APP_CONFIG_HPP

#include "MainNode.hpp"
#include "app/Menu.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"

class ScrapbookAppConfig : public AppConfigurable
{
public:
  explicit ScrapbookAppConfig(PlatformContext *context)
      : AppConfigurable(context)
  {
  }

  virtual void compose(AppComposition &composition)
  {
    composition << WindowDef(this->productionWindowProps(
        loka::app::scene::Boundary<scrapbook::MainNode>(
            scrapbook::MainProps().platformContext(this->getPlatformContext()))));
  }

  virtual void composeMenu(loka::app::MenuComposition &composition)
  {
    using namespace loka::app;
    composition.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP) << MenuSeparator()
                                  << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
  }

protected:
  /** Declares ScrapbookUI's production window presentation around a supplied
      scene so non-production vehicles cannot drift its title or frame. */
  WindowProps productionWindowProps(const loka::app::scene::NodeDefinitionBase &scene) const
  {
    return WindowProps()
        .frame(40, 40, 340, 250)
        .scene(scene)
        .title("ScrapbookUI")
        .visible(true);
  }
};

#endif // LOKA_SCRAPBOOK_UI_APP_CONFIG_HPP
