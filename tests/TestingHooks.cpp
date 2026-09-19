// Implementations of TEST_BUILD-only failure-injection hooks declared in
// library and scenario-test headers. They live in a test translation unit
// on purpose: platform core libraries (LokaWin32Core etc.) are built without
// TEST_BUILD and must stay hook-free, while every test executable compiles
// this file via LOKA_SHARED_TEST_SOURCES.
#include "app/Menu.hpp"
#include "core/LokaAlloc.hpp"
#include "support/LokaAllocFailure.hpp"
#include <cstring>
#include <cassert>
#include "scenarios/ObservedMainDefinition.hpp"
#include "scenarios/ScenarioReel.hpp"

#ifdef TEST_BUILD

namespace loka
{
  namespace core
  {
    namespace testing
    {
      namespace
      {
        const char *g_failureOwner = 0;
        const char *g_failureType = 0;
        int g_allocationFailures = 0;
        int g_allocationLive = 0;
        int g_allocationAttempts = 0;

        void *allocateWithFailures(std::size_t size, const LokaAllocationSite &site)
        {
          ++g_allocationAttempts;
          if (g_allocationFailures > 0 && site.ownerTag && site.typeTag
              && std::strcmp(site.ownerTag, g_failureOwner) == 0 && std::strcmp(site.typeTag, g_failureType) == 0)
          {
            --g_allocationFailures;
            return 0;
          }
          void *memory = new (std::nothrow) char[size];
          if (memory)
            ++g_allocationLive;
          return memory;
        }

        void freeWithFailures(void *memory, const LokaAllocationSite &)
        {
          --g_allocationLive;
          delete[] static_cast<char *>(memory);
        }
      } // namespace

      void failLokaAllocRaw(const char *owner, const char *type, int count)
      {
        g_failureOwner = owner;
        g_failureType = type;
        g_allocationFailures = count;
        g_allocationAttempts = 0;
        LokaAllocSetBackend(&allocateWithFailures, &freeWithFailures);
      }

      void allowLokaAllocRaw()
      {
        assert(g_allocationLive == 0);
        LokaAllocSetBackend(0, 0);
        g_allocationFailures = 0;
      }

      int lokaAllocRawLive()
      {
        return g_allocationLive;
      }
      int lokaAllocRawAttempts()
      {
        return g_allocationAttempts;
      }
    } // namespace testing
  } // namespace core

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
        int g_scenarioReelDriverAllocationFailures = 0;
      } // namespace

      void failScenarioReelDriverAllocations(int count)
      {
        g_scenarioReelDriverAllocationFailures = count;
      }

      void allowScenarioReelDriverAllocations()
      {
        g_scenarioReelDriverAllocationFailures = 0;
      }

      bool shouldAllocateScenarioReelDriver()
      {
        if (g_scenarioReelDriverAllocationFailures > 0)
        {
          --g_scenarioReelDriverAllocationFailures;
          return false;
        }
        return true;
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
