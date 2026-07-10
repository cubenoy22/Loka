#ifndef LOKA_CORE2_SCENE_DETAIL_ARENAMATH_HPP
#define LOKA_CORE2_SCENE_DETAIL_ARENAMATH_HPP

#include <cstddef>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {
        // C++98 alignof alternative shared by arena sizing and allocation.
        template <typename T> struct AlignOfHelper
        {
          char c;
          T t;
        };

        template <typename T> struct AlignOf
        {
          enum
          {
            value = sizeof(AlignOfHelper<T>) - sizeof(T)
          };
        };

        /** The effective alignment the arenas (NodeArena, StateArena) use for an
            allocation request: raised to at least sizeof(void*) and rounded up to
            a power of two. Reserve estimates must normalize through this same
            function so an estimate is never smaller than the allocation it
            mirrors -- raw AlignOf can sit below the arena minimum (e.g. 2-byte
            natural alignment on 68k against a 4-byte pointer). */
        inline size_t NormalizeArenaAlign(size_t align)
        {
          size_t minAlign = sizeof(void *);
          if (minAlign < 2)
          {
            minAlign = 2;
          }
          if (align < minAlign)
          {
            align = minAlign;
          }
          if ((align & (align - 1)) != 0)
          {
            size_t p2 = 1;
            while (p2 < align)
            {
              p2 <<= 1;
            }
            align = p2;
          }
          return align;
        }
      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_DETAIL_ARENAMATH_HPP
