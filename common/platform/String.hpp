#ifndef LOKA_PLATFORM_STRING_HPP
#define LOKA_PLATFORM_STRING_HPP

#include <cstddef>
#include <string>

#include "core/Managed.hpp"

namespace loka
{
  namespace platform
  {
    class GraphemeString
    {
    public:
      virtual ~GraphemeString() {}
      virtual std::size_t length() const = 0;
      virtual bool rangeAt(std::size_t index, std::size_t &start, std::size_t &length) const = 0;
    };

    /** Borrowed UTF-8 bytes, valid while the owning String lives. No terminator
     * is required; an empty view may have either null or non-null bytes. */
    struct Utf8View
    {
      const char *bytes;
      std::size_t length;
    };

    class String
    {
    public:
      virtual ~String() {}
      /** On true, out is exactly the bytes a successful appendUtf8 would append.
       * On false, out is unchanged. The view borrows from this String. */
      virtual bool queryUtf8(Utf8View &out) const
      {
        (void)out;
        return false;
      }
      virtual bool appendUtf8(std::string &out) const = 0;
      virtual loka::core::Managed<GraphemeString> createGraphemeString() const
      {
        return loka::core::Managed<GraphemeString>();
      }
    };

    loka::core::Managed<String> CreatePlatformStringFromUtf8(const char *bytes, std::size_t length);
    loka::core::Managed<String> CreatePlatformStringFromLiteral(const char *literal);
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_STRING_HPP
