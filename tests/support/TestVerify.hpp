#ifndef LOKA_TESTS_SUPPORT_TESTVERIFY_HPP
#define LOKA_TESTS_SUPPORT_TESTVERIFY_HPP

#include <cstdio>
#include <cstdlib>

/** Checks a condition whose expression also performs the work the test needs.
    `assert` compiles its whole expression out under NDEBUG, so a call placed
    inside one silently stops happening there — the test then reads whatever
    the uninitialized or empty out-parameter holds.

    This evaluates the expression exactly once and fails the test in every
    build, NDEBUG included: a test binary exists to discriminate, and a
    load-bearing step that fails must fail the run, not hand garbage to the
    next line. (Pure comparisons stay plain `assert`; their absence under
    NDEBUG loses discrimination but never corrupts the test's own execution.)

    Use it whenever the expression builds, writes, opens, registers, flushes,
    or fills an out-parameter. Keep plain `assert` for pure comparisons. */
// A verified expression is sometimes a compile-time fact (`sizeof(A) ==
// sizeof(B)`). MSVC 19.44 reports C4127 for the resulting constant `if`, and
// the warnings-as-errors presets turn that into a build failure the hosted
// 19.51 runner never shows. Suppress it at the one place the `if` is written
// instead of reshaping each such test (#586 did that per site).
#if defined(_MSC_VER)
#define LOKA_VERIFY_CONSTANT_CONDITION_OK __pragma(warning(suppress : 4127))
#else
#define LOKA_VERIFY_CONSTANT_CONDITION_OK
#endif

#define LOKA_VERIFY(expr)                                                      \
  do                                                                           \
  {                                                                            \
    LOKA_VERIFY_CONSTANT_CONDITION_OK                                          \
    if (!(expr))                                                               \
    {                                                                          \
      std::fprintf(stderr, "%s:%d: LOKA_VERIFY failed: %s\n", __FILE__,        \
                   __LINE__, #expr);                                           \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

#endif // LOKA_TESTS_SUPPORT_TESTVERIFY_HPP
