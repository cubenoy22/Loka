#include "RailMetricsTests.hpp"
#include "app/layout/RailMetrics.hpp"
#include "support/TestVerify.hpp"
#if defined(__linux__) && !defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

void testRailMetricsValueSemantics()
{
  using namespace loka::app;
  const RailMetrics defaults;
  LOKA_VERIFY(defaults.fontScale == Ratio(1, 1));
  LOKA_VERIFY(defaults.spaceScale == Ratio(1, 1));
  LOKA_VERIFY(defaults.fontScale.valid());
  LOKA_VERIFY(defaults.spaceScale.isUnit());
  LOKA_VERIFY(!Ratio(3, 2).isUnit());
  LOKA_VERIFY(Ratio(2, 2).isUnit());
  const RailMetrics scaled(Ratio(5, 4), Ratio(3, 2));
  RailMetrics copy = scaled;
  LOKA_VERIFY(copy == scaled);
  LOKA_VERIFY(copy != defaults);
  LOKA_VERIFY(RailMetrics(Ratio(5, 4), Ratio()) != defaults);
  LOKA_VERIFY(RailMetrics(Ratio(), Ratio(3, 2)) != defaults);
  LOKA_VERIFY(Ratio(3, 2) != Ratio(4, 2));
  // Rails only scale up: a ratio below one is not a valid RailMetrics value.
  LOKA_VERIFY(!Ratio(3, 4).valid());
  LOKA_VERIFY(!Ratio(1, 4).valid());
  LOKA_VERIFY(Ratio(1, 1).valid() && Ratio(5, 4).valid());
  copy = defaults;
  LOKA_VERIFY(copy == defaults);
  LOKA_VERIFY(scaled.spaceScale == Ratio(3, 2));
  LOKA_VERIFY(Ratio(3, 2) != Ratio(6, 4)); // Representation equality.

  Ratio invalid;
  invalid.den = 0;
  LOKA_VERIFY(!invalid.valid());
  LOKA_VERIFY(!invalid.isUnit());
  invalid.den = -1;
  LOKA_VERIFY(!invalid.valid());
  invalid.den = 1;
  invalid.num = 0;
  LOKA_VERIFY(!invalid.valid());
  invalid.num = -1;
  LOKA_VERIFY(!invalid.valid());
#ifdef NDEBUG
  LOKA_VERIFY(!Ratio(1, 0).valid());
  LOKA_VERIFY(!Ratio(1, -1).valid());
  LOKA_VERIFY(RailMetrics(Ratio(), Ratio(3, 4)).spaceScale.isUnit()); // refused to unit in release
#elif defined(__linux__) && !defined(__SANITIZE_ADDRESS__)
  // Construction of a Ratio never aborts; RailMetrics refuses an invalid
  // ratio (den <= 0, or below one) with an assert in debug and the unit
  // ratio in every build.
  const Ratio refusedRatios[] = {Ratio(1, 0), Ratio(1, -1), Ratio(3, 4)};
  for (unsigned int i = 0; i < sizeof(refusedRatios) / sizeof(refusedRatios[0]); ++i)
  {
    const pid_t child = fork();
    LOKA_VERIFY(child >= 0);
    if (child == 0)
    {
      const RailMetrics refused(Ratio(), refusedRatios[i]);
      (void)refused;
      _exit(0);
    }
    int status = 0;
    LOKA_VERIFY(waitpid(child, &status, 0) == child);
    LOKA_VERIFY(WIFSIGNALED(status));
    LOKA_VERIFY(WTERMSIG(status) == SIGABRT);
  }
#else
  std::printf("[skip] Ratio constructor death pin requires Linux debug without ASan; valid() covered here.\n");
#endif
}
