#ifndef LOKA_CORE_UTIL_MEMORY_HPP
#define LOKA_CORE_UTIL_MEMORY_HPP

#include <stddef.h>

namespace loka
{
  namespace core
  {
    namespace util
    {
      inline bool BytesEqual(const void *left, const void *right, size_t count)
      {
        const unsigned char *leftBytes = static_cast<const unsigned char *>(left);
        const unsigned char *rightBytes = static_cast<const unsigned char *>(right);
        for (size_t i = 0; i < count; ++i)
        {
          if (leftBytes[i] != rightBytes[i])
          {
            return false;
          }
        }
        return true;
      }
    } // namespace util
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_UTIL_MEMORY_HPP
