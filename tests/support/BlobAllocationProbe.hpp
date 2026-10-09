#ifndef LOKA_TEST_BLOB_ALLOCATION_PROBE_HPP
#define LOKA_TEST_BLOB_ALLOCATION_PROBE_HPP
#include <cstring>
#include <new>
#include "core/LokaAlloc.hpp"
#include "support/TestVerify.hpp"

namespace
{
  struct BlobAllocationProbe
  {
    BlobAllocationProbe()
        : live(0),
          attempts(0),
          byteFrees(0),
          refusals(0),
          skip(0),
          remaining(0),
          owner("Blob"),
          type("Bytes")
    {
      current() = this;
      loka::core::LokaAllocSetBackend(&allocate, &release);
    }
    ~BlobAllocationProbe()
    {
      LOKA_VERIFY(live == 0);
      loka::core::LokaAllocSetBackend(0, 0);
      current() = 0;
    }
    void refuse(const char *o, const char *t, int count = 1, int before = 0)
    {
      owner = o;
      type = t;
      remaining = count;
      skip = before;
    }
    static BlobAllocationProbe *&current()
    {
      static BlobAllocationProbe *value = 0;
      return value;
    }
    static void *allocate(std::size_t n, const loka::core::LokaAllocationSite &site)
    {
      BlobAllocationProbe &p = *current();
      ++p.attempts;
      if (p.remaining && std::strcmp(site.ownerTag, p.owner) == 0 && std::strcmp(site.typeTag, p.type) == 0)
      {
        if (p.skip)
          --p.skip;
        else
        {
          --p.remaining;
          ++p.refusals;
          return 0;
        }
      }
      void *memory = new (std::nothrow) char[n];
      if (memory)
        ++p.live;
      return memory;
    }
    static void release(void *memory, const loka::core::LokaAllocationSite &site)
    {
      BlobAllocationProbe &p = *current();
      --p.live;
      if (std::strcmp(site.ownerTag, "Blob") == 0 && std::strcmp(site.typeTag, "Bytes") == 0)
        ++p.byteFrees;
      delete[] static_cast<char *>(memory);
    }
    int live, attempts, byteFrees, refusals, skip, remaining;
    const char *owner;
    const char *type;

  private:
    BlobAllocationProbe(const BlobAllocationProbe &);
    BlobAllocationProbe &operator=(const BlobAllocationProbe &);
  };
} // namespace
#endif
