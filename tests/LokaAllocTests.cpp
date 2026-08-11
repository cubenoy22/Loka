#include "LokaAllocTests.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

#include "core/LokaAlloc.hpp"
#include "support/TestVerify.hpp"

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
  (void)constructedBefore;
  assert(gGateProbeConstructed == constructedBefore + 1);

  loka::core::LokaDelete(probe, gateProbeSite());
  (void)destroyedBefore;
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
  (void)constructedBefore;
  assert(gGateProbeConstructed == constructedBefore + 1);

  GateProbe *second = loka::core::LokaNew<GateProbe>(gateProbeSite(), 2);
  (void)second;
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
  (void)destroyedBefore;
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

void testLokaAllocCensusAccumulatesSitesAndLabelsOverflow()
{
#if defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
  static const char *overflowTypeTags[] = {
      "Overflow00", "Overflow01", "Overflow02", "Overflow03",
      "Overflow04", "Overflow05", "Overflow06", "Overflow07",
      "Overflow08", "Overflow09", "Overflow10", "Overflow11",
      "Overflow12", "Overflow13", "Overflow14", "Overflow15",
      "Overflow16", "Overflow17", "Overflow18", "Overflow19",
      "Overflow20", "Overflow21", "Overflow22", "Overflow23",
      "Overflow24", "Overflow25", "Overflow26", "Overflow27",
      "Overflow28", "Overflow29", "Overflow30", "Overflow31",
      "Overflow32", "Overflow33"};
  const loka::core::LokaAllocationSite primarySite("CensusProbe", "Primary");

  void *first = loka::core::LokaAllocRaw(7, primarySite);
  void *second = loka::core::LokaAllocRaw(11, primarySite);
  LOKA_VERIFY(first != 0);
  LOKA_VERIFY(second != 0);
  loka::core::LokaFreeRaw(first, primarySite);
  loka::core::LokaFreeRaw(second, primarySite);

  for (std::size_t i = 0; i < sizeof(overflowTypeTags) / sizeof(overflowTypeTags[0]); ++i)
  {
    const loka::core::LokaAllocationSite overflowSite("CensusProbe", overflowTypeTags[i]);
    void *storage = loka::core::LokaAllocRaw(1, overflowSite);
    LOKA_VERIFY(storage != 0);
    loka::core::LokaFreeRaw(storage, overflowSite);
  }

  std::FILE *dump = std::tmpfile();
  LOKA_VERIFY(dump != 0);
  loka::core::LokaAllocCensusDump(dump);
  std::rewind(dump);

  char line[160];
  bool foundPrimary = false;
  bool foundOverflow = false;
  while (std::fgets(line, sizeof(line), dump))
  {
    if (std::strcmp(line, "alloc.site.CensusProbe.Primary=2,18\n") == 0)
      foundPrimary = true;
    if (std::strcmp(line, "alloc.site.overflow=3,3\n") == 0)
      foundOverflow = true;
  }
  std::fclose(dump);

  LOKA_VERIFY(foundPrimary);
  LOKA_VERIFY(foundOverflow);
#endif
  std::printf("==== [testLokaAllocCensusAccumulatesSitesAndLabelsOverflow] PASSED ====\n");
}
