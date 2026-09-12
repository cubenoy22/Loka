#include "support/UpstreamGaugePin.hpp"
#include "support/TestVerify.hpp"
#include "support/OrdinaryFlowPin.hpp"

namespace
{
  // Test-only host simulation; malloc has no path back through operator new.
  struct UpstreamPinSource
  {
    enum { kAlignment = 16 };
    static void *acquire(size_t size) { return std::malloc(size); }
    static void release(void *p) { std::free(p); }
  };
  static loka::core::SmallObjectPool<UpstreamPinSource> upstreamPinPool;
  static void *upstreamPinAllocate(std::size_t size) { return upstreamPinPool.allocate(size); }
  static void upstreamPinRelease(void *p) { upstreamPinPool.release(p); }
}

loka::core::UpstreamGauge upstreamPinSnapshot() { return upstreamPinPool.snapshot(); }

void upstreamPinCheck(const char *name, const loka::core::UpstreamGauge &before,
                      const loka::core::UpstreamGauge &after,
                      unsigned long attempts, unsigned long bytes)
{
  LOKA_VERIFY(!before.saturated && !after.saturated);
  LOKA_VERIFY(after.attempts >= before.attempts && after.successes >= before.successes);
  LOKA_VERIFY(after.bytesAcquired >= before.bytesAcquired && after.failures >= before.failures);
  std::fprintf(stderr, "upstream %s: attempts=%lu successes=%lu bytes=%lu failures=%lu\n",
               name, after.attempts - before.attempts, after.successes - before.successes,
               after.bytesAcquired - before.bytesAcquired, after.failures - before.failures);
  LOKA_VERIFY(after.attempts - before.attempts <= attempts);
  LOKA_VERIFY(after.successes - before.successes <= attempts);
  LOKA_VERIFY(after.bytesAcquired - before.bytesAcquired <= bytes);
  LOKA_VERIFY(after.failures - before.failures == 0);
}

namespace
{
  void runUpstreamFlowPin()
  {
    int input = 10;
    loka::core::MutableState<int> result;
    loka::core::PushStateTracker tracker;
    loka::dsl::FlowChain<int, int> chain = ordinary_flow_pin::make(&input, &result, &tracker);
    LOKA_VERIFY(chain.run() && result.get() == 20);
    LOKA_VERIFY(chain.run() && result.get() == 20);
    const loka::core::UpstreamGauge before = upstreamPinSnapshot();
    input = 20;
    LOKA_VERIFY(chain.run() && result.get() == 40);
    upstreamPinCheck("Flow", before, upstreamPinSnapshot(), 0, 0);
  }
}
