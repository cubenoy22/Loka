#ifndef LOKA_CORE_STRING_BUFFER_HPP
#define LOKA_CORE_STRING_BUFFER_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "loka/core/Managed.hpp"

namespace loka
{
  namespace platform
  {
    class String;
    class GraphemeString;
  }

  namespace core
  {
    enum StringEncoding
    {
      StringEncodingUtf8 = 0,
      StringEncodingUtf16,
      StringEncodingUtf32
    };

    class StringBuffer
    {
    public:
      typedef unsigned short Utf16Unit;
      typedef unsigned int Utf32Unit;

      StringBuffer();
      explicit StringBuffer(StringEncoding encoding);

      StringEncoding encoding() const;
      unsigned char characterSize() const;
      std::size_t length() const;
      unsigned int characterAt(std::size_t index) const;
      const void *data() const;
      void *data();
      void clear();
      void reserve(std::size_t units);
      void resize(std::size_t units);
      bool assignFromUtf8(const std::string &utf8);
      bool assignFromUtf8(const std::string &utf8, bool allowResize);
      static bool CountUnitsFromUtf8(const std::string &utf8, StringEncoding encoding, std::size_t &units);
      const loka::core::Managed<platform::String> &platformHandle() const;
      loka::core::Managed<platform::GraphemeString> graphemeHandle() const;
      void setPlatformHandle(const loka::core::Managed<platform::String> &handle);

    private:
      StringEncoding encoding_;
      loka::core::Managed<platform::String> platformHandle_;
      std::vector<unsigned char> utf8_;
      std::vector<Utf16Unit> utf16_;
      std::vector<Utf32Unit> utf32_;
    };

  } // namespace core
} // namespace loka

#endif // LOKA_CORE_STRING_BUFFER_HPP
