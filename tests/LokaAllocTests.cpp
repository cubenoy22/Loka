#include "LokaAllocTests.hpp"

#include <cassert>
#include <cstddef>
#include <new>

#include "core/LokaAlloc.hpp"

namespace
{
  int gGateProbeConstructed = 0;
  int gGateProbeDestroyed = 0;

  struct GateProbe
  {
    explicit GateProbe(int valueIn)
        : value(valueIn)
    {
      ++gGateProbeConstructed;
    }

    ~GateProbe()
    {
      ++gGateProbeDestroyed;
    }

    int value;
  };

  const loka::core::LokaAllocationSite &gateProbeSite()
  {
    static const loka::core::LokaAllocationSite site("LokaAllocTests", "GateProbe");
    return site;
  }

  // Fake backend: honors the contract (may refuse; a refusal is final) while
  // counting traffic so tests can prove which backend served a call.
  int gFakeBackendAllocCalls = 0;
  int gFakeBackendFreeCalls = 0;
  int gFakeBackendRefusalIndex = 0; // 1-based call index to refuse; 0 refuses nothing

  void *fakeBackendAlloc(std::size_t size, const loka::core::LokaAllocationSite &site)
  {
    (void)site;
    ++gFakeBackendAllocCalls;
    if (gFakeBackendAllocCalls == gFakeBackendRefusalIndex)
      return 0;
    return new (std::nothrow) char[size];
  }

  void fakeBackendFree(void *ptr, const loka::core::LokaAllocationSite &site)
  {
    (void)site;
    ++gFakeBackendFreeCalls;
    delete[] static_cast<char *>(ptr);
  }
} // namespace

void testLokaAllocDefaultBackendRoundTrip()
{
  const int constructedBefore = gGateProbeConstructed;
  const int destroyedBefore = gGateProbeDestroyed;

  GateProbe *probe = loka::core::LokaNew<GateProbe>(gateProbeSite(), 7);
  assert(probe != 0);
  assert(probe->value == 7);
  assert(gGateProbeConstructed == constructedBefore + 1);

  loka::core::LokaDelete(probe, gateProbeSite());
  assert(gGateProbeDestroyed == destroyedBefore + 1);

  // LokaDelete is null-safe: releasing an OOM white flag runs no destructor.
  loka::core::LokaDelete<GateProbe>(0, gateProbeSite());
  assert(gGateProbeDestroyed == destroyedBefore + 1);
}

void testLokaNewReturnsNullWhenBackendRefusesNthAllocation()
{
  const int constructedBefore = gGateProbeConstructed;
  const int destroyedBefore = gGateProbeDestroyed;
#ifdef LOKA_LIFECYCLE_AUDIT
  const int liveBefore = loka::core::LokaAllocAuditLiveCount(gateProbeSite());
#endif

  gFakeBackendAllocCalls = 0;
  gFakeBackendFreeCalls = 0;
  gFakeBackendRefusalIndex = 2;
  loka::core::LokaAllocSetBackend(&fakeBackendAlloc, &fakeBackendFree);

  GateProbe *first = loka::core::LokaNew<GateProbe>(gateProbeSite(), 1);
  assert(first != 0);
  assert(gFakeBackendAllocCalls == 1);
  assert(gGateProbeConstructed == constructedBefore + 1);

  GateProbe *second = loka::core::LokaNew<GateProbe>(gateProbeSite(), 2);
  assert(second == 0);
  assert(gFakeBackendAllocCalls == 2);
  // The white flag constructs nothing: only the first probe ever existed.
  assert(gGateProbeConstructed == constructedBefore + 1);
#ifdef LOKA_LIFECYCLE_AUDIT
  // A refused allocation never enters the ledger.
  assert(loka::core::LokaAllocAuditLiveCount(gateProbeSite()) == liveBefore + 1);
#endif

  loka::core::LokaDelete(first, gateProbeSite());
  assert(gFakeBackendFreeCalls == 1);
  assert(gGateProbeDestroyed == destroyedBefore + 1);
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditLiveCount(gateProbeSite()) == liveBefore);
#endif

  loka::core::LokaAllocSetBackend(0, 0);
  gFakeBackendRefusalIndex = 0;
}

void testLokaAllocBackendResetRestoresDefault()
{
  gFakeBackendAllocCalls = 0;
  gFakeBackendFreeCalls = 0;
  gFakeBackendRefusalIndex = 0;
  loka::core::LokaAllocSetBackend(&fakeBackendAlloc, &fakeBackendFree);
  loka::core::LokaAllocSetBackend(0, 0);

  GateProbe *probe = loka::core::LokaNew<GateProbe>(gateProbeSite(), 3);
  assert(probe != 0);
  loka::core::LokaDelete(probe, gateProbeSite());

  // The default backend served the round trip after the reset.
  assert(gFakeBackendAllocCalls == 0);
  assert(gFakeBackendFreeCalls == 0);
}

void testLokaAllocAuditBalancedUseCountsToZero()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  // The checkpoint aborts on failure, so this test only drives the balanced
  // path; the outstanding-probe queries below are how a leak would surface.
  const int liveBefore = loka::core::LokaAllocAuditLiveCount(gateProbeSite());
  const int totalBefore = loka::core::LokaAllocAuditTotalLiveCount();

  GateProbe *outstanding = loka::core::LokaNew<GateProbe>(gateProbeSite(), 4);
  assert(outstanding != 0);
  assert(loka::core::LokaAllocAuditLiveCount(gateProbeSite()) == liveBefore + 1);
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalBefore + 1);

  loka::core::LokaDelete(outstanding, gateProbeSite());
  assert(loka::core::LokaAllocAuditLiveCount(gateProbeSite()) == liveBefore);
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalBefore);

  loka::core::LokaAllocAuditCheckpoint("testLokaAllocAuditBalancedUseCountsToZero");
#endif
}
