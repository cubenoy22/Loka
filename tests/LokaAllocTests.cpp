#include "LokaAllocTests.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

#include "core/LokaAlloc.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "support/LokaAllocFailure.hpp"
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

#if defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
  struct CensusSnapshot
  {
    CensusSnapshot()
        : entryCount(0),
          overflowCount(0),
          overflowBytes(0)
    {
    }

    unsigned long entryCount;
    unsigned long overflowCount;
    unsigned long overflowBytes;
  };

  bool readCensusSnapshot(CensusSnapshot &snapshot)
  {
    std::FILE *dump = std::tmpfile();
    if (!dump)
      return false;

    loka::core::LokaAllocCensusDump(dump);
    std::rewind(dump);

    char line[160];
    while (std::fgets(line, sizeof(line), dump))
    {
      unsigned long count = 0;
      unsigned long bytes = 0;
      if (std::sscanf(line, "alloc.site.overflow=%lu,%lu", &count, &bytes) == 2)
      {
        snapshot.overflowCount = count;
        snapshot.overflowBytes = bytes;
      }
      else if (std::strncmp(line, "alloc.site.", 11) == 0)
      {
        ++snapshot.entryCount;
      }
    }

    std::fclose(dump);
    return true;
  }
#endif
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
  const int liveBefore = loka::core::LokaAllocAuditLiveCount(gateProbeSite());
  const int totalBefore = loka::core::LokaAllocAuditTotalLiveCount();

  GateProbe *outstanding = loka::core::LokaNew<GateProbe>(gateProbeSite(), 4);
  assert(outstanding != 0);
  assert(loka::core::LokaAllocAuditLiveCount(gateProbeSite()) == liveBefore + 1);
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalBefore + 1);

  loka::core::LokaDelete(outstanding, gateProbeSite());
  assert(loka::core::LokaAllocAuditLiveCount(gateProbeSite()) == liveBefore);
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalBefore);

#endif
}

void testLokaAllocCensusAccumulatesSitesAndLabelsOverflow()
{
#if defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
  static const char *typeTags[] = {
      "Conservation00", "Conservation01", "Conservation02", "Conservation03", "Conservation04", "Conservation05",
      "Conservation06", "Conservation07", "Conservation08", "Conservation09", "Conservation10", "Conservation11",
      "Conservation12", "Conservation13", "Conservation14", "Conservation15", "Conservation16", "Conservation17",
      "Conservation18", "Conservation19", "Conservation20", "Conservation21", "Conservation22", "Conservation23",
      "Conservation24", "Conservation25", "Conservation26", "Conservation27", "Conservation28", "Conservation29",
      "Conservation30", "Conservation31", "Conservation32", "Conservation33"};
  const std::size_t siteCount = sizeof(typeTags) / sizeof(typeTags[0]);
  CensusSnapshot before;
  LOKA_VERIFY(readCensusSnapshot(before));

  for (std::size_t i = 0; i < siteCount; ++i)
  {
    const loka::core::LokaAllocationSite site("CensusProbe", typeTags[i]);
    void *storage = loka::core::LokaAllocRaw(i + 1, site);
    LOKA_VERIFY(storage != 0);
    loka::core::LokaFreeRaw(storage, site);
  }

  CensusSnapshot after;
  LOKA_VERIFY(readCensusSnapshot(after));
  LOKA_VERIFY(after.entryCount >= before.entryCount);
  LOKA_VERIFY(after.overflowCount >= before.overflowCount);
  LOKA_VERIFY(after.overflowBytes >= before.overflowBytes);

  const unsigned long entriesGained = after.entryCount - before.entryCount;
  const unsigned long overflowCountGained = after.overflowCount - before.overflowCount;
  LOKA_VERIFY(overflowCountGained != 0);
  LOKA_VERIFY(entriesGained + overflowCountGained == siteCount);

  unsigned long expectedOverflowBytes = 0;
  for (std::size_t i = entriesGained; i < siteCount; ++i)
    expectedOverflowBytes += static_cast<unsigned long>(i + 1);
  LOKA_VERIFY(after.overflowBytes - before.overflowBytes == expectedOverflowBytes);
#endif
  std::printf("==== [testLokaAllocCensusAccumulatesSitesAndLabelsOverflow] PASSED ====\n");
}

void testBlobOwnedWriteLeavesNoLiveGateAllocation()
{
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalBefore = loka::core::LokaAllocAuditTotalLiveCount();
  const int managedBefore = loka::core::LokaAllocAuditLiveCount(
      loka::core::ManagedControlBlockSite());
  {
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    blob.mutableBytes().push_back(42);
    assert(blob.size() == 1);
    assert(blob.data()[0] == 42);
  }
  assert(loka::core::LokaAllocAuditLiveCount(
      loka::core::ManagedControlBlockSite()) == managedBefore);
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalBefore);
#endif
}

void testBlobEmptyWritePreservesSharedRecord()
{
  loka::core::resource::Blob empty = loka::core::resource::Blob::Empty();
  loka::core::resource::Blob writable = empty;
  writable.mutableBytes().push_back(42);
  assert(writable != empty);
  assert(empty.data() == 0);
  assert(empty == loka::core::resource::Blob::Empty());
  assert(writable.data()[0] == 42);
}

namespace
{
  void countImageRelease(void *, void *userData)
  {
    ++*static_cast<int *>(userData);
  }

  // FromNative consumes the native handle: a refusal must hand back an invalid
  // Image and release the handle exactly once, never abort (#1064).
  void verifyImageFromNativeRefusal(const char *owner, const char *type)
  {
    int releases = 0;
    loka::core::testing::failLokaAllocRaw(owner, type, 1);
    {
      const loka::core::resource::Image image = loka::core::resource::Image::FromNative(
          reinterpret_cast<void *>(1), 4, 3, &countImageRelease, &releases);
      LOKA_VERIFY(!image.isValid());
      LOKA_VERIFY(image.nativeHandle() == 0);
      LOKA_VERIFY(releases == 1);
    }
    LOKA_VERIFY(releases == 1);
    LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
    loka::core::testing::allowLokaAllocRaw();
  }
} // namespace

void testImageFromNativeRecordRefusalReleasesNativeOnce()
{
  verifyImageFromNativeRefusal("Image", "Record");
}

void testImageFromNativeControlBlockRefusalReleasesNativeOnce()
{
  verifyImageFromNativeRefusal("Managed", "ControlBlock");
}

void testImageFromNativeReleasesNativeOnceAfterLastCopy()
{
  int releases = 0;
  loka::core::testing::failLokaAllocRaw("Image", "Record", 0);
  {
    const loka::core::resource::Image image = loka::core::resource::Image::FromNative(
        reinterpret_cast<void *>(1), 4, 3, &countImageRelease, &releases);
    LOKA_VERIFY(image.isValid());
    LOKA_VERIFY(image.width() == 4 && image.height() == 3);
    {
      const loka::core::resource::Image copy = image;
      LOKA_VERIFY(copy == image);
    }
    LOKA_VERIFY(releases == 0);
  }
  LOKA_VERIFY(releases == 1);
  LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
  loka::core::testing::allowLokaAllocRaw();
}
