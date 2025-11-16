#ifndef DECLARA_CORE_STRINGS_STRINGS_HPP
#define DECLARA_CORE_STRINGS_STRINGS_HPP

// C++98-friendly string primitives and conversions

#include <cstddef>
#include <string>

namespace declara
{
  namespace core
  {
    namespace strings
    {

      // CSSTR: C-style zero-terminated UTF-8 string alias
      typedef const char *CSSTR;

      // StringSlice: lightweight view (like string_view) over UTF-8 bytes
      struct StringSlice
      {
        const char *data;
        std::size_t length;
        StringSlice() : data(0), length(0) {}
        StringSlice(const char *p, std::size_t n) : data(p), length(n) {}
        explicit StringSlice(const std::string &s) : data(s.empty() ? 0 : s.c_str()), length(s.size()) {}
        bool empty() const { return length == 0 || data == 0; }
      };

      // Pascal8: length-prefixed (1-byte) string view (classic Mac-style)
      struct Pascal8
      {
        const unsigned char *bytes;
        // bytes[0] holds length, followed by data
        Pascal8() : bytes(0) {}
        explicit Pascal8(const unsigned char *p) : bytes(p) {}
        unsigned int len() const { return bytes ? static_cast<unsigned int>(bytes[0]) : 0; }
        const char *cdata() const { return bytes ? reinterpret_cast<const char *>(bytes + 1) : 0; }
      };

      // Pascal16: length-prefixed (2-byte, big endian by convention here) string view
      struct Pascal16
      {
        const unsigned char *bytes;
        Pascal16() : bytes(0) {}
        explicit Pascal16(const unsigned char *p) : bytes(p) {}
        unsigned int len() const
        {
          if (!bytes)
            return 0;
          return (static_cast<unsigned int>(bytes[0]) << 8) | static_cast<unsigned int>(bytes[1]);
        }
        const char *cdata() const { return bytes ? reinterpret_cast<const char *>(bytes + 2) : 0; }
      };

      // Convert Pascal8 to slice
      inline StringSlice toSlice(const Pascal8 &p)
      {
        return StringSlice(p.cdata(), p.len());
      }

      // Convert Pascal16 to slice
      inline StringSlice toSlice(const Pascal16 &p)
      {
        return StringSlice(p.cdata(), p.len());
      }

// UTF-8 <-> UTF-16 (wchar_t on Windows) conversions
#ifdef _WIN32
#include <windows.h>

      // Returns true on success. Does not write beyond outCap. Always null-terminates if space permits.
      inline bool utf8_to_wide(const char *utf8, std::size_t utf8Len, wchar_t *out, std::size_t outCap, std::size_t *written)
      {
        if (!out || outCap == 0)
          return false;
        int need = MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(utf8Len), NULL, 0);
        if (need <= 0)
          return false;
        if (static_cast<std::size_t>(need) >= outCap)
          return false;
        int done = MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(utf8Len), out, need);
        if (done != need)
          return false;
        out[need] = L'\0';
        if (written)
          *written = static_cast<std::size_t>(need);
        return true;
      }

      inline bool wide_to_utf8(const wchar_t *w, std::size_t wLen, char *out, std::size_t outCap, std::size_t *written)
      {
        if (!out || outCap == 0)
          return false;
        int need = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(wLen), NULL, 0, NULL, NULL);
        if (need <= 0)
          return false;
        if (static_cast<std::size_t>(need) >= outCap)
          return false;
        int done = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(wLen), out, need, NULL, NULL);
        if (done != need)
          return false;
        out[need] = '\0';
        if (written)
          *written = static_cast<std::size_t>(need);
        return true;
      }

      // Convenience wrappers for std::string/std::wstring
      inline bool utf8_to_wstring(const std::string &s, std::wstring &out)
      {
        if (s.empty())
        {
          out.assign(L"");
          return true;
        }
        int need = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), NULL, 0);
        if (need <= 0)
          return false;
        out.resize(need);
        int done = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], need);
        return done == need;
      }

      inline bool wstring_to_utf8(const std::wstring &w, std::string &out)
      {
        if (w.empty())
        {
          out.assign("");
          return true;
        }
        int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), NULL, 0, NULL, NULL);
        if (need <= 0)
          return false;
        out.resize(need);
        int done = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], need, NULL, NULL);
        return done == need;
      }

#else // _WIN32

      // Minimal stubs for non-Windows platforms: perform byte copy (assume UTF-8 everywhere)
      inline bool utf8_to_wide(const char * /*utf8*/, std::size_t /*utf8Len*/, wchar_t * /*out*/, std::size_t /*outCap*/, std::size_t * /*written*/)
      {
        return false; // Not implemented yet
      }
      inline bool wide_to_utf8(const wchar_t * /*w*/, std::size_t /*wLen*/, char * /*out*/, std::size_t /*outCap*/, std::size_t * /*written*/)
      {
        return false; // Not implemented yet
      }

#endif // _WIN32

    } // namespace strings
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE_STRINGS_STRINGS_HPP
