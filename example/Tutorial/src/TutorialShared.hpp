#ifndef LOKA_TUTORIAL_SHARED_HPP
#define LOKA_TUTORIAL_SHARED_HPP

#include "app/nodes/Text.hpp"
#include "app/Menu.hpp"

namespace tutorial
{
  inline loka::app::TextDefinition TutorialTitle(const char *title)
  {
    return loka::app::Text(title);
  }

  inline loka::app::TextDefinition TutorialHint(const char *text)
  {
    return loka::app::Text(text);
  }

  /** Declares the Tutorial application menu shared by its shipping and
      standalone presentation configs. */
  inline void DeclareTutorialMenu(loka::app::MenuComposition &composition)
  {
    using namespace loka::app;
    composition.declare(AppMenu()                                 //
                        << MenuItem("About")                      //
                               .actionType(MENU_ACTION_ABOUT_APP) //
                        << MenuSeparator()                        //
                        << MenuItem("Quit")                       //
                               .actionType(MENU_ACTION_QUIT_APP));
  }
} // namespace tutorial

#endif // LOKA_TUTORIAL_SHARED_HPP
