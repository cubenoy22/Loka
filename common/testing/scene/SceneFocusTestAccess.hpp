#ifndef LOKA_TESTING_SCENE_FOCUS_TEST_ACCESS_HPP
#define LOKA_TESTING_SCENE_FOCUS_TEST_ACCESS_HPP
#include "app/scene/SceneFocus.hpp"
#if defined(TEST_BUILD)
namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct SceneFocusTestAccess
      {
        static SceneFocus *owner(FocusRow &row)
        {
          return row.owner_;
        }
        static FocusRow *head(SceneFocus &focus)
        {
          return focus.head_;
        }
        static FocusRow *next(FocusRow &row)
        {
          return row.next_;
        }
        static void join(SceneFocus &focus, FocusRow &row)
        {
          focus.join(row);
        }
        static void publish(SceneFocus &focus, FocusRow &row)
        {
          focus.published_.connect(row.publication_);
        }
        static void source(FocusLink &reader, FocusRow &row)
        {
          reader.connect(row.source_);
        }
        static bool published(FocusRow &row)
        {
          return row.publication_.peer_ != 0;
        }
        static bool sourced(FocusRow &row)
        {
          return row.source_.peer_ != 0;
        }
        static void duringPublication(SceneFocus &focus, void (*action)(void *), void *data)
        {
          SceneFocus::Publication scope(focus);
          action(data);
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif

#endif
