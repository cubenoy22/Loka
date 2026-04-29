#ifndef LOKA_TUTORIAL_SHARED_HPP
#define LOKA_TUTORIAL_SHARED_HPP

#include "app/nodes/Text.hpp"

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
} // namespace tutorial

#endif // LOKA_TUTORIAL_SHARED_HPP
