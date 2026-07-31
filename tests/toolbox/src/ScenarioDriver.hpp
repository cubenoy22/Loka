#ifndef LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_HPP
#define LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_HPP

namespace loka
{
  namespace toolbox_tests
  {
    int RunScenarioApplication();

    /** Reached only after the App and PlatformContext are destroyed; the
        watchpoint harness breaks here to bound its teardown watch window. */
    void ScenarioTeardownComplete();
  }
} // namespace loka

#endif // LOKA_TESTS_TOOLBOX_SCENARIO_DRIVER_HPP
