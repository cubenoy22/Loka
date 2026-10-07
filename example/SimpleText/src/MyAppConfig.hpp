#ifndef LOKA_SIMPLE_TEXT_APP_CONFIG_HPP
#define LOKA_SIMPLE_TEXT_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/Menu.hpp"
#include "MainNode.hpp"

class SimpleTextAppConfig : public AppConfigurable
{
public:
  explicit SimpleTextAppConfig(PlatformContext *context)
      : AppConfigurable(context)
  {
  }
  virtual void compose(AppComposition &c)
  {
    c << WindowDef(
        WindowProps()
            .frame(16, 16, 480, 320)
            .title("LokaSimpleText")
            .visible(true)
            .scene(loka::app::scene::Boundary<simpletext::MainNode>(simpletext::MainProps()
                                                                        .platformContext(this->getPlatformContext())
                                                                        .newEvent(&this->newEvent_)
                                                                        .openEvent(&this->openEvent_)
                                                                        .saveEvent(&this->saveEvent_)
                                                                        .saveAsEvent(&this->saveAsEvent_))));
  }
  virtual void composeMenu(loka::app::MenuComposition &c)
  {
    using namespace loka::app;
    c << (Menu("File") << MenuItem("New").onClick(&this->newEvent_)
                       << MenuItem("Open...").shortcut('o').onClick(&this->openEvent_)
                       << MenuItem("Save").shortcut('s').onClick(&this->saveEvent_)
                       << MenuItem("Save As...").onClick(&this->saveAsEvent_) << MenuSeparator()
                       << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
  }

private:
#ifdef TEST_BUILD
  friend class ::SimpleTextTestAccess;
#endif
  loka::core::EmitterState newEvent_, openEvent_, saveEvent_, saveAsEvent_;
};
#endif
