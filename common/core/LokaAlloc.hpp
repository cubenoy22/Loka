#ifndef LOKA_CORE_LOKAALLOC_HPP
#define LOKA_CORE_LOKAALLOC_HPP

#include <cstddef>
#include <new>

namespace loka
{
  namespace core
  {
    /** Identifies the logical owner and resident type at an allocation gate. */
    struct LokaAllocationSite
    {
      LokaAllocationSite(const char *ownerTagValue, const char *typeTagValue)
          : ownerTag(ownerTagValue),
            typeTag(typeTagValue)
      {
      }

      const char *ownerTag;
      const char *typeTag;
    };

    /**
     * Raw-memory backend seam behind the gate. Exactly one process-static
     * alloc/free pair serves every gate allocation; platforms and test fakes
     * inject through LokaAllocSetBackend. A 0 result means the backend
     * already gave up: the backend may fight before surrendering (purge,
     * GrowZone delegation, ...), but that policy lives entirely in the
     * backend — never in the gate or its callers. The site accompanies the
     * free as well so audit and future site-routing backends stay balanced.
     */
    typedef void *(*LokaAllocBackendAllocFn)(std::size_t size, const LokaAllocationSite &site);
    typedef void (*LokaAllocBackendFreeFn)(void *ptr, const LokaAllocationSite &site);

    /**
     * Injects the backend pair. The pair travels together: passing 0 for
     * either function restores the default backend (a nothrow new[]/delete[]
     * equivalent). Storage must return through the backend that produced it,
     * so injectors only swap backends around allocation-balanced regions.
     * Process-static and single-threaded like the rest of the gate.
     */
    void LokaAllocSetBackend(LokaAllocBackendAllocFn allocFn, LokaAllocBackendFreeFn freeFn);

    /**
     * Acquires size bytes of raw storage from the current backend. Returns 0
     * when the backend already gave up; the caller converts 0 into its own
     * failure unit and propagates it — no layer swallows the flag.
     */
    void *LokaAllocRaw(std::size_t size, const LokaAllocationSite &site);

    /**
     * Returns storage acquired with LokaAllocRaw to the current backend
     * under the same site. Null is ignored and never reaches the backend.
     */
    void LokaFreeRaw(void *ptr, const LokaAllocationSite &site);

#ifdef LOKA_LIFECYCLE_AUDIT
    /** Gate allocations currently live against this site's tags, 0 if never seen. */
    int LokaAllocAuditLiveCount(const LokaAllocationSite &site);
    /** Gate allocations currently live across every site. */
    int LokaAllocAuditTotalLiveCount();
    /** Prints every site with a nonzero live count to stderr. */
    void LokaAllocAuditDump();
    /** Aborts after printing all nonzero sites when any gate allocation is still live. */
    void LokaAllocAuditCheckpoint(const char *label);
#endif

    /**
     * Allocates an identity-bearing chain resident through Loka's allocation
     * gate. Storage comes from the backend seam and the object is constructed
     * in place on success; the site is explicit so the backend can route by
     * owner and type without changing call sites. Returns 0 when the backend
     * already gave up — nothing is constructed. The caller owns a non-null
     * result and releases it with LokaDelete under the same site.
     */
    template <class T> T *LokaNew(const LokaAllocationSite &site)
    {
      void *storage = LokaAllocRaw(sizeof(T), site);
      if (!storage)
        return 0;
      return new (storage) T();
    }

    template <class T, class A1> T *LokaNew(const LokaAllocationSite &site, const A1 &a1)
    {
      void *storage = LokaAllocRaw(sizeof(T), site);
      if (!storage)
        return 0;
      return new (storage) T(a1);
    }

    template <class T, class A1, class A2>
    T *LokaNew(const LokaAllocationSite &site, const A1 &a1, const A2 &a2)
    {
      void *storage = LokaAllocRaw(sizeof(T), site);
      if (!storage)
        return 0;
      return new (storage) T(a1, a2);
    }

    template <class T, class A1, class A2, class A3>
    T *LokaNew(const LokaAllocationSite &site, const A1 &a1, const A2 &a2, const A3 &a3)
    {
      void *storage = LokaAllocRaw(sizeof(T), site);
      if (!storage)
        return 0;
      return new (storage) T(a1, a2, a3);
    }

    /**
     * Destroys a gate-allocated object and returns its storage to the
     * backend. Null-safe so callers can release unconditionally. The pointer
     * must be the exact one LokaNew returned and the site must match, so the
     * audit ledger stays balanced.
     */
    template <class T> void LokaDelete(T *object, const LokaAllocationSite &site)
    {
      if (!object)
        return;
      object->~T();
      LokaFreeRaw(object, site);
    }
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_LOKAALLOC_HPP
