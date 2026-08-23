#ifndef LOKA_TESTS_STANDALONE_PERFORMANCE_CONFIG_HPP
#define LOKA_TESTS_STANDALONE_PERFORMANCE_CONFIG_HPP

/** Complete Standalone Flow application passes to measure. */
#ifndef LOKA_STANDALONE_PERFORMANCE_RUNS
#define LOKA_STANDALONE_PERFORMANCE_RUNS 0
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

#endif // LOKA_TESTS_STANDALONE_PERFORMANCE_CONFIG_HPP
