#ifndef LOKA_CORE_LOKAALLOC_HPP
#define LOKA_CORE_LOKAALLOC_HPP

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
     * Allocates an identity-bearing chain resident through Loka's allocation
     * gate. The site is explicit now so a later backend can route by owner and
     * type without changing call sites. The caller owns a non-null result.
     */
    template <class T> T *LokaNew(const LokaAllocationSite &site)
    {
      (void)site;
      return new (std::nothrow) T();
    }

    template <class T, class A1> T *LokaNew(const LokaAllocationSite &site, const A1 &a1)
    {
      (void)site;
      return new (std::nothrow) T(a1);
    }

    template <class T, class A1, class A2>
    T *LokaNew(const LokaAllocationSite &site, const A1 &a1, const A2 &a2)
    {
      (void)site;
      return new (std::nothrow) T(a1, a2);
    }

    template <class T, class A1, class A2, class A3>
    T *LokaNew(const LokaAllocationSite &site, const A1 &a1, const A2 &a2, const A3 &a3)
    {
      (void)site;
      return new (std::nothrow) T(a1, a2, a3);
    }
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_LOKAALLOC_HPP
