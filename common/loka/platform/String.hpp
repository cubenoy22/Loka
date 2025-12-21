#ifndef LOKA_PLATFORM_STRING_HPP
#define LOKA_PLATFORM_STRING_HPP

#include <cstddef>
#include <string>

#include "core/Managed.hpp"

namespace loka
{
  namespace platform
  {
    class String
    {
    public:
      virtual ~String() {}
      virtual bool appendUtf8(std::string &out) const = 0;
    };

    Managed<String> CreatePlatformStringFromUtf8(const char *bytes, std::size_t length);
    Managed<String> CreatePlatformStringFromLiteral(const char *literal);
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_STRING_HPP
