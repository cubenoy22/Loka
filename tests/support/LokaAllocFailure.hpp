#ifndef LOKA_TESTS_SUPPORT_LOKA_ALLOC_FAILURE_HPP
#define LOKA_TESTS_SUPPORT_LOKA_ALLOC_FAILURE_HPP

#ifdef TEST_BUILD
namespace loka
{
  namespace core
  {
    namespace testing
    {
      // Install only around allocation-balanced regions, like LokaAllocSetBackend.
      void failLokaAllocRaw(const char *owner, const char *type, int count);
      void allowLokaAllocRaw();
      int lokaAllocRawLive();
      int lokaAllocRawAttempts();
    } // namespace testing
  } // namespace core
} // namespace loka
#endif
#endif
