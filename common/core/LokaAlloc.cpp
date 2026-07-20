#include "core/LokaAlloc.hpp"

#ifdef LOKA_LIFECYCLE_AUDIT
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

namespace loka
{
  namespace core
  {
    namespace
    {
      void *LokaAllocDefaultBackendAlloc(std::size_t size, const LokaAllocationSite &site)
      {
        (void)site;
        return new (std::nothrow) char[size];
      }

      void LokaAllocDefaultBackendFree(void *ptr, const LokaAllocationSite &site)
      {
        (void)site;
        delete[] static_cast<char *>(ptr);
      }

      LokaAllocBackendAllocFn gLokaAllocBackendAlloc = &LokaAllocDefaultBackendAlloc;
      LokaAllocBackendFreeFn gLokaAllocBackendFree = &LokaAllocDefaultBackendFree;

#ifdef LOKA_LIFECYCLE_AUDIT
      // Sites are static call sites, so a small fixed table covers them; a
      // linear scan is fine at that scale and overflow is a programming error.
      enum
      {
        kMaxLokaAllocAuditSites = 64
      };

      struct LokaAllocAuditEntry
      {
        const char *ownerTag;
        const char *typeTag;
        int live;
      };

      LokaAllocAuditEntry gLokaAllocAuditEntries[kMaxLokaAllocAuditSites];
      int gLokaAllocAuditEntryCount = 0;
      int gLokaAllocAuditTotalLive = 0;

      // Tags match by content, not pointer identity, because equal literals
      // are not guaranteed to be pooled across translation units.
      bool LokaAllocAuditTagsEqual(const char *lhs, const char *rhs)
      {
        if (lhs == rhs)
          return true;
        if (!lhs || !rhs)
          return false;
        return std::strcmp(lhs, rhs) == 0;
      }

      LokaAllocAuditEntry *FindLokaAllocAuditEntry(const LokaAllocationSite &site)
      {
        for (int i = 0; i < gLokaAllocAuditEntryCount; ++i)
        {
          if (LokaAllocAuditTagsEqual(gLokaAllocAuditEntries[i].ownerTag, site.ownerTag) &&
              LokaAllocAuditTagsEqual(gLokaAllocAuditEntries[i].typeTag, site.typeTag))
            return &gLokaAllocAuditEntries[i];
        }
        return 0;
      }

      LokaAllocAuditEntry *FindOrCreateLokaAllocAuditEntry(const LokaAllocationSite &site)
      {
        LokaAllocAuditEntry *entry = FindLokaAllocAuditEntry(site);
        if (entry)
          return entry;

        if (gLokaAllocAuditEntryCount >= kMaxLokaAllocAuditSites)
        {
          assert(false && "Loka alloc audit site table overflow");
          std::abort();
        }
        entry = &gLokaAllocAuditEntries[gLokaAllocAuditEntryCount++];
        entry->ownerTag = site.ownerTag;
        entry->typeTag = site.typeTag;
        entry->live = 0;
        return entry;
      }

      void LokaAllocAuditRecordAlloc(const LokaAllocationSite &site)
      {
        LokaAllocAuditEntry *entry = FindOrCreateLokaAllocAuditEntry(site);
        if (entry->live == INT_MAX || gLokaAllocAuditTotalLive == INT_MAX)
        {
          assert(false && "Loka alloc audit counter overflow");
          std::abort();
        }
        ++entry->live;
        ++gLokaAllocAuditTotalLive;
      }

      void LokaAllocAuditRecordFree(const LokaAllocationSite &site)
      {
        LokaAllocAuditEntry *entry = FindLokaAllocAuditEntry(site);
        assert(entry && "Loka alloc audit free without matching alloc");
        if (!entry)
          std::abort();

        if (entry->live <= 0 || gLokaAllocAuditTotalLive <= 0)
        {
          assert(false && "Loka alloc audit counter underflow");
          std::abort();
        }
        --entry->live;
        --gLokaAllocAuditTotalLive;
      }
#endif // LOKA_LIFECYCLE_AUDIT
    } // namespace

    void LokaAllocSetBackend(LokaAllocBackendAllocFn allocFn, LokaAllocBackendFreeFn freeFn)
    {
      if (!allocFn || !freeFn)
      {
        gLokaAllocBackendAlloc = &LokaAllocDefaultBackendAlloc;
        gLokaAllocBackendFree = &LokaAllocDefaultBackendFree;
        return;
      }
      gLokaAllocBackendAlloc = allocFn;
      gLokaAllocBackendFree = freeFn;
    }

    void *LokaAllocRaw(std::size_t size, const LokaAllocationSite &site)
    {
      void *storage = gLokaAllocBackendAlloc(size, site);
#ifdef LOKA_LIFECYCLE_AUDIT
      if (storage)
        LokaAllocAuditRecordAlloc(site);
#endif
      return storage;
    }

    void LokaFreeRaw(void *ptr, const LokaAllocationSite &site)
    {
      if (!ptr)
        return;
#ifdef LOKA_LIFECYCLE_AUDIT
      LokaAllocAuditRecordFree(site);
#endif
      gLokaAllocBackendFree(ptr, site);
    }

#ifdef LOKA_LIFECYCLE_AUDIT
    int LokaAllocAuditLiveCount(const LokaAllocationSite &site)
    {
      const LokaAllocAuditEntry *entry = FindLokaAllocAuditEntry(site);
      return entry ? entry->live : 0;
    }

    int LokaAllocAuditTotalLiveCount()
    {
      return gLokaAllocAuditTotalLive;
    }

    void LokaAllocAuditDump()
    {
      std::fprintf(stderr, "[Loka alloc audit] owner\ttype\tlive\n");
      for (int i = 0; i < gLokaAllocAuditEntryCount; ++i)
      {
        const LokaAllocAuditEntry &entry = gLokaAllocAuditEntries[i];
        if (entry.live != 0)
        {
          std::fprintf(stderr, "%s\t%s\t%d\n",
                       entry.ownerTag ? entry.ownerTag : "(null)",
                       entry.typeTag ? entry.typeTag : "(null)", entry.live);
        }
      }
    }

    void LokaAllocAuditCheckpoint(const char *label)
    {
      if (gLokaAllocAuditTotalLive == 0)
        return;

      std::fprintf(stderr, "[Loka alloc audit] checkpoint failed: %s\n", label ? label : "(unnamed)");
      LokaAllocAuditDump();
      std::abort();
    }
#endif // LOKA_LIFECYCLE_AUDIT
  } // namespace core
} // namespace loka
