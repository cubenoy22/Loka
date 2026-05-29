#ifndef LOKA_WIN32_PLATFORM_STRING_HPP
#define LOKA_WIN32_PLATFORM_STRING_HPP

#include <string>

#include "core/Managed.hpp"
#include "loka/core/String.hpp"
#include "platform/String.hpp"

namespace loka
{
  namespace win32
  {
    // Win32-specific string implementation that stores UTF-16 data.
    class Win32String : public loka::platform::String
    {
    public:
      Win32String();
      explicit Win32String(const std::wstring &value);

      const wchar_t *c_str() const;
      std::size_t length() const;
      bool empty() const;

      virtual bool appendUtf8(std::string &out) const;
      virtual loka::core::Managed<loka::platform::GraphemeString> createGraphemeString() const;

    private:
      std::wstring storage_;
    };

    // Materialize a logical loka::core::String into a Win32String (UTF-16).
    loka::core::Managed<loka::platform::String> CreateWin32String(const loka::core::String &logical);
    loka::core::Managed<loka::platform::String> CreateWin32StringFromUtf16(const wchar_t *chars, std::size_t length);
    bool MaterializeWideString(const loka::core::String &logical, std::wstring &out);

  } // namespace win32
} // namespace loka

#endif // LOKA_WIN32_PLATFORM_STRING_HPP
