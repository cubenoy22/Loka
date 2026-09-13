#ifndef LOKA_TOOLBOX_MEMORY_SOURCE_HPP
#define LOKA_TOOLBOX_MEMORY_SOURCE_HPP

#include <MacMemory.h>
#include <cstddef>
#include <limits>
#include <stdint.h>

namespace loka
{
  namespace toolbox
  {
    /** Memory Manager storage for every target linking this Classic rail.
     * release accepts only a non-null live acquisition, never a pool slot.
     * On 68K, acquire adds four bytes and O(1) arithmetic plus one byte store;
     * release recovers the native block with one byte load and subtraction.
     * Paid once per refill or individual request, with no additional row walk.
     */
    struct ClassicMemorySource
    {
      // PPC retains its native eight-byte promise. Older 68000 Memory Managers
      // promise only even addresses; the 68K source supplies four-byte alignment
      // for both pool chunks and individual large/direct/fallback requests.
#if defined(__ppc__) || defined(__POWERPC__)
      enum { kAlignment = 8 };
#else
      enum { kAlignment = 4 };
#endif

      static void *acquire(std::size_t size)
      {
#if defined(__ppc__) || defined(__POWERPC__)
        return NewPtr(size);
#else
        // Size is signed: refuse before either addition or native conversion.
        if (size > static_cast<std::size_t>((std::numeric_limits<Size>::max)()) - kAlignment)
          return 0;
        Ptr original = NewPtr(static_cast<Size>(size + kAlignment));
        if (!original) return 0;
        const unsigned char distance = static_cast<unsigned char>(
            kAlignment - (reinterpret_cast<uintptr_t>(original) & (kAlignment - 1)));
        Ptr aligned = original + distance;
        // Always advance 1..4 bytes, reserving a byte even for an aligned input.
        // This prefix belongs to the source block, never to individual pool slots.
        aligned[-1] = static_cast<char>(distance);
        return aligned;
#endif
      }

      static void release(void *storage)
      {
#if defined(__ppc__) || defined(__POWERPC__)
        DisposePtr(static_cast<Ptr>(storage));
#else
        // Paired with acquire's byte prefix; chunks stay retained by the pool.
        Ptr aligned = static_cast<Ptr>(storage);
        DisposePtr(aligned - static_cast<unsigned char>(aligned[-1]));
#endif
      }
    };
  }
}
#endif
