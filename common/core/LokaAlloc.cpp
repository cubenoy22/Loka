#include "core/LokaAlloc.hpp"

#if defined(LOKA_LIFECYCLE_AUDIT) || defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
#include <climits>
#include <cstdio>
#include <cstring>
#endif

#ifdef LOKA_LIFECYCLE_AUDIT
#include <cassert>
#include <cstdlib>
#endif

namespace loka
{
  namespace core
  {
    namespace
    {
#if defined(LOKA_LIFECYCLE_AUDIT) || defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
      // Tags match by content, not pointer identity, because equal literals
      // are not guaranteed to be pooled across translation units.
      bool LokaAllocationTagsEqual(const char *lhs, const char *rhs)
      {
        if (lhs == rhs)
          return true;
        if (!lhs || !rhs)
          return false;
        return std::strcmp(lhs, rhs) == 0;
      }
#endif

#if defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
      struct LokaAllocCensusEntry
      {
        const char *ownerTag;
        const char *typeTag;
        unsigned long count;
        unsigned long bytes;
      };

      class LokaAllocCensus
      {
      public:
        LokaAllocCensus()
            : entryCount_(0),
              overflowCount_(0),
              overflowBytes_(0)
        {
        }

        void record(const LokaAllocationSite &site, std::size_t size)
        {
          LokaAllocCensusEntry *entry = this->find(site);
          if (!entry && this->entryCount_ < kMaxSites)
          {
            entry = &this->entries_[this->entryCount_++];
            entry->ownerTag = site.ownerTag;
            entry->typeTag = site.typeTag;
            entry->count = 0;
            entry->bytes = 0;
          }
          if (!entry)
          {
            add(this->overflowCount_, 1);
            add(this->overflowBytes_, size);
            return;
          }
          add(entry->count, 1);
          add(entry->bytes, size);
        }

        void dump(std::FILE *fp) const
        {
          if (!fp)
            return;
          for (int i = 0; i < this->entryCount_; ++i)
          {
            const LokaAllocCensusEntry &entry = this->entries_[i];
            std::fprintf(fp, "alloc.site.%s.%s=%lu,%lu\n",
                         entry.ownerTag ? entry.ownerTag : "(null)",
                         entry.typeTag ? entry.typeTag : "(null)",
                         entry.count, entry.bytes);
          }
          if (this->overflowCount_ != 0)
          {
            std::fprintf(fp, "alloc.site.overflow=%lu,%lu\n",
                         this->overflowCount_,
                         this->overflowBytes_);
          }
        }

      private:
        enum
        {
          kMaxSites = 32
        };

        static void add(unsigned long &total, std::size_t value)
        {
          if (value > static_cast<std::size_t>(ULONG_MAX) ||
              total > ULONG_MAX - static_cast<unsigned long>(value))
          {
            total = ULONG_MAX;
            return;
          }
          total += static_cast<unsigned long>(value);
        }

        LokaAllocCensusEntry *find(const LokaAllocationSite &site)
        {
          for (int i = 0; i < this->entryCount_; ++i)
          {
            LokaAllocCensusEntry &entry = this->entries_[i];
            if (LokaAllocationTagsEqual(entry.ownerTag, site.ownerTag) &&
                LokaAllocationTagsEqual(entry.typeTag, site.typeTag))
              return &entry;
          }
          return 0;
        }

        LokaAllocCensusEntry entries_[kMaxSites];
        int entryCount_;
        unsigned long overflowCount_;
        unsigned long overflowBytes_;
      };

      LokaAllocCensus gLokaAllocCensus;
#endif

      void *LokaAllocDefaultBackendAlloc(std::size_t size, const LokaAllocationSite &site)
      {
#if defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
        void *storage = new (std::nothrow) char[size];
        if (storage)
          gLokaAllocCensus.record(site, size);
        return storage;
#else
        (void)site;
        return new (std::nothrow) char[size];
#endif
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

      LokaAllocAuditEntry *FindLokaAllocAuditEntry(const LokaAllocationSite &site)
      {
        for (int i = 0; i < gLokaAllocAuditEntryCount; ++i)
        {
          if (LokaAllocationTagsEqual(gLokaAllocAuditEntries[i].ownerTag, site.ownerTag) &&
              LokaAllocationTagsEqual(gLokaAllocAuditEntries[i].typeTag, site.typeTag))
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

#if defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
    void LokaAllocCensusDump(std::FILE *fp)
    {
      gLokaAllocCensus.dump(fp);
    }
#endif

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
