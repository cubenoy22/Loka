#include "ReclaimScratchTests.hpp"
#include "support/ReclaimGauge.hpp"
#include "support/TestVerify.hpp"
#include "core/SmallObjectPool.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

namespace
{
  bool denied = false;
  unsigned long allocations = 0;
  struct Source
  {
    enum
    {
      kAlignment = 16
    };
    static void *acquire(size_t n)
    {
      if (denied)
      {
        std::fprintf(stderr, "reclaim Source DENIED: %lu bytes\n", static_cast<unsigned long>(n));
        return 0;
      }
      return std::malloc(n);
    }
    static void release(void *p)
    {
      std::free(p);
    }
  };
  loka::core::SmallObjectPool<Source> pool;
  loka::core::UpstreamGauge before;
  unsigned long allocationsBefore = 0;
  void *allocate(size_t size)
  {
    ++allocations;
    return pool.allocate(size);
  }
} // namespace
void reclaimGaugeBegin()
{
  before = pool.snapshot();
  allocationsBefore = allocations;
  denied = true;
}
void reclaimGaugeEnd()
{
  denied = false;
  const loka::core::UpstreamGauge after = pool.snapshot();
  std::fprintf(stderr,
               "reclaim gauge: attempts=%lu successes=%lu bytes=%lu failures=%lu new=%lu\n",
               after.attempts - before.attempts,
               after.successes - before.successes,
               after.bytesAcquired - before.bytesAcquired,
               after.failures - before.failures,
               allocations - allocationsBefore);
  LOKA_VERIFY(!before.saturated && !after.saturated);
  LOKA_VERIFY(after.attempts == before.attempts && after.successes == before.successes);
  LOKA_VERIFY(after.bytesAcquired == before.bytesAcquired && after.failures == before.failures);
  LOKA_VERIFY(allocations == allocationsBefore);
}
void *operator new(size_t size) throw(std::bad_alloc)
{
  void *p = allocate(size ? size : 1);
  if (!p)
  {
    reclaimGaugeEnd();
    std::abort();
  }
  return p;
}
void *operator new[](size_t size) throw(std::bad_alloc)
{
  return ::operator new(size);
}
void *operator new(size_t size, const std::nothrow_t &) throw()
{
  return allocate(size ? size : 1);
}
void *operator new[](size_t size, const std::nothrow_t &) throw()
{
  return allocate(size ? size : 1);
}
void operator delete(void *p) throw()
{
  pool.release(p);
}
void operator delete[](void *p) throw()
{
  pool.release(p);
}
void operator delete(void *p, const std::nothrow_t &) throw()
{
  pool.release(p);
}
void operator delete[](void *p, const std::nothrow_t &) throw()
{
  pool.release(p);
}
#if __cplusplus >= 201402L
void operator delete(void *p, size_t) throw()
{
  pool.release(p);
}
void operator delete[](void *p, size_t) throw()
{
  pool.release(p);
}
#endif
int main(int argc, char **argv)
{
  const char *names[] = {"boundary-wide",
                         "boundary-deep",
                         "generation-wide",
                         "generation-deep",
                         "partition-wide",
                         "partition-deep",
                         "overflow",
                         "unattached"};
  void (*tests[])() = {testReclaimScratchBoundaryWide,
                       testReclaimScratchBoundaryDeep,
                       testReclaimScratchGenerationWide,
                       testReclaimScratchGenerationDeep,
                       testReclaimScratchPartitionWide,
                       testReclaimScratchPartitionDeep,
                       testReclaimScratchOverflow,
                       testReclaimScratchPartitionUnattached};
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    if (argc == 1 || std::strcmp(argv[1], names[i]) == 0)
    {
      std::fprintf(stderr, "%s\n", names[i]);
      tests[i]();
    }
  return 0;
}
