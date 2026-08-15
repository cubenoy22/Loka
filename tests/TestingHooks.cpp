// Implementations of TEST_BUILD-only failure-injection hooks declared in
// library and scenario-test headers. They live in a test translation unit
// on purpose: platform core libraries (LokaWin32Core etc.) are built without
// TEST_BUILD and must stay hook-free, while every test executable compiles
// this file via LOKA_SHARED_TEST_SOURCES.
#include "app/Menu.hpp"
#include "scenarios/ObservedMainDefinition.hpp"

#ifdef TEST_BUILD

namespace loka
{
  namespace app
  {
    namespace testing
    {
      namespace
      {
        int g_menuBarDefinitionCloneFailures = 0;
      }

      void failNextMenuBarDefinitionClone()
      {
        g_menuBarDefinitionCloneFailures = 1;
      }

      void failMenuBarDefinitionClones(int count)
      {
        g_menuBarDefinitionCloneFailures = count;
      }

      void allowMenuBarDefinitionClones()
      {
        g_menuBarDefinitionCloneFailures = 0;
      }

      bool shouldCloneMenuBarDefinition()
      {
        if (g_menuBarDefinitionCloneFailures > 0)
        {
          --g_menuBarDefinitionCloneFailures;
          return false;
        }
        return true;
      }
    } // namespace testing
  } // namespace app

  namespace scenario_tests
  {
    namespace testing
    {
      namespace
      {
        int g_observedMainDefinitionCloneFailures = 0;
      }

      void failObservedMainDefinitionClones(int count)
      {
        g_observedMainDefinitionCloneFailures = count;
      }

      void allowObservedMainDefinitionClones()
      {
        g_observedMainDefinitionCloneFailures = 0;
      }

      bool shouldCloneObservedMainDefinition()
      {
        if (g_observedMainDefinitionCloneFailures > 0)
        {
          --g_observedMainDefinitionCloneFailures;
          return false;
        }
        return true;
      }
    } // namespace testing
  } // namespace scenario_tests
} // namespace loka

#endif // TEST_BUILD
