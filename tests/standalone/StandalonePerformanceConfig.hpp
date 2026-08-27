#ifndef LOKA_TESTS_STANDALONE_PERFORMANCE_CONFIG_HPP
#define LOKA_TESTS_STANDALONE_PERFORMANCE_CONFIG_HPP

/** Complete Standalone Flow application passes to measure. */
#ifndef LOKA_STANDALONE_PERFORMANCE_RUNS
#define LOKA_STANDALONE_PERFORMANCE_RUNS 0
#endif

/** Re-arm a completed Standalone Flow Scene until the user closes its Window. */
#ifndef LOKA_STANDALONE_FLOW_LOOP
#define LOKA_STANDALONE_FLOW_LOOP 0
#endif

namespace loka
{
  namespace standalone_tests
  {
    enum
    {
      kConfiguredPerformanceRuns = LOKA_STANDALONE_PERFORMANCE_RUNS
    };
  }
}

#if LOKA_STANDALONE_PERFORMANCE_RUNS != 0                                                                              \
    && (LOKA_STANDALONE_PERFORMANCE_RUNS < 3 || LOKA_STANDALONE_PERFORMANCE_RUNS > 10)
#error LOKA_STANDALONE_PERFORMANCE_RUNS must be 0 or between 3 and 10
#endif

#if LOKA_STANDALONE_PERFORMANCE_RUNS > 0 && LOKA_STANDALONE_FLOW_LOOP
#error Standalone Flow performance measurement and autonomous looping are mutually exclusive
#endif

#endif // LOKA_TESTS_STANDALONE_PERFORMANCE_CONFIG_HPP
