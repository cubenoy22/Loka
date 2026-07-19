#include <cstdio>
#include <cstring>
#include "core/diag/LifecycleAudit.hpp"

namespace
{
  typedef void (*LokaTestFunction)();

  struct LokaRegisteredTest
  {
    const char *name;
    LokaTestFunction function;
  };

#if defined(LOKA_TEST_RUNNER_CONTRACT) || defined(LOKA_TEST_RUNNER_COMBINED)
#define LOKA_TEST(testFunction) {#testFunction, &testFunction},
#define LOKA_SCENARIO_TESTS_HERE() {0, 0},
  const LokaRegisteredTest lokaContractTests[] = {
#include "ContractTestRegistry.inc"
  };
#undef LOKA_SCENARIO_TESTS_HERE
#undef LOKA_TEST
#endif

#if defined(LOKA_TEST_RUNNER_SCENARIO) || defined(LOKA_TEST_RUNNER_COMBINED)
#define LOKA_TEST(testFunction) {#testFunction, &testFunction},
  const LokaRegisteredTest lokaScenarioTests[] = {
#include "ScenarioTestRegistry.inc"
  };
#undef LOKA_TEST
#endif

  void lokaRunTest(const LokaRegisteredTest &test)
  {
    test.function();
    LOKA_AUDIT_CHECKPOINT(test.name);
  }

#if defined(LOKA_TEST_RUNNER_SCENARIO) || defined(LOKA_TEST_RUNNER_COMBINED)
  void lokaRunAllScenarioTests()
  {
    const std::size_t count = sizeof(lokaScenarioTests) / sizeof(lokaScenarioTests[0]);
    for (std::size_t i = 0; i < count; ++i)
    {
      lokaRunTest(lokaScenarioTests[i]);
    }
  }

  void lokaListAllScenarioTests()
  {
    const std::size_t count = sizeof(lokaScenarioTests) / sizeof(lokaScenarioTests[0]);
    for (std::size_t i = 0; i < count; ++i)
    {
      std::printf("%s\n", lokaScenarioTests[i].name);
    }
  }

  const LokaRegisteredTest *lokaFindScenarioTest(const char *name)
  {
    const std::size_t count = sizeof(lokaScenarioTests) / sizeof(lokaScenarioTests[0]);
    for (std::size_t i = 0; i < count; ++i)
    {
      if (std::strcmp(name, lokaScenarioTests[i].name) == 0)
      {
        return &lokaScenarioTests[i];
      }
    }
    return 0;
  }
#endif

  void lokaRunAllTests()
  {
#if defined(LOKA_TEST_RUNNER_CONTRACT) || defined(LOKA_TEST_RUNNER_COMBINED)
    const std::size_t contractCount = sizeof(lokaContractTests) / sizeof(lokaContractTests[0]);
    for (std::size_t i = 0; i < contractCount; ++i)
    {
      if (lokaContractTests[i].function)
      {
        lokaRunTest(lokaContractTests[i]);
      }
#if defined(LOKA_TEST_RUNNER_COMBINED)
      else
      {
        lokaRunAllScenarioTests();
      }
#endif
    }
#elif defined(LOKA_TEST_RUNNER_SCENARIO)
    lokaRunAllScenarioTests();
#endif
    LOKA_AUDIT_CHECKPOINT(LOKA_TEST_RUNNER_FINAL_CHECKPOINT);
  }

  void lokaListAllTests()
  {
#if defined(LOKA_TEST_RUNNER_CONTRACT) || defined(LOKA_TEST_RUNNER_COMBINED)
    const std::size_t contractCount = sizeof(lokaContractTests) / sizeof(lokaContractTests[0]);
    for (std::size_t i = 0; i < contractCount; ++i)
    {
      if (lokaContractTests[i].function)
      {
        std::printf("%s\n", lokaContractTests[i].name);
      }
#if defined(LOKA_TEST_RUNNER_COMBINED)
      else
      {
        lokaListAllScenarioTests();
      }
#endif
    }
#elif defined(LOKA_TEST_RUNNER_SCENARIO)
    lokaListAllScenarioTests();
#endif
  }

  const LokaRegisteredTest *lokaFindTest(const char *name)
  {
#if defined(LOKA_TEST_RUNNER_CONTRACT) || defined(LOKA_TEST_RUNNER_COMBINED)
    const std::size_t contractCount = sizeof(lokaContractTests) / sizeof(lokaContractTests[0]);
    for (std::size_t i = 0; i < contractCount; ++i)
    {
      if (lokaContractTests[i].function && std::strcmp(name, lokaContractTests[i].name) == 0)
      {
        return &lokaContractTests[i];
      }
    }
#endif
#if defined(LOKA_TEST_RUNNER_SCENARIO) || defined(LOKA_TEST_RUNNER_COMBINED)
    return lokaFindScenarioTest(name);
#else
    return 0;
#endif
  }
} // namespace

int main(int argc, char **argv)
{
  if (argc == 1)
  {
    lokaRunAllTests();
    return 0;
  }

  if (argc == 2 && std::strcmp(argv[1], "--list") == 0)
  {
    lokaListAllTests();
    return 0;
  }

  if (argc == 2)
  {
    const LokaRegisteredTest *test = lokaFindTest(argv[1]);
    if (test)
    {
      lokaRunTest(*test);
      return 0;
    }
    std::fprintf(stderr, "Unknown test: %s\n", argv[1]);
    return 1;
  }

  std::fprintf(stderr, "Usage: %s [--list|test-name]\n", argv[0]);
  return 1;
}

