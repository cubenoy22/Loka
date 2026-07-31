#include "lrpc/Utf8Path.hpp"

#include <climits>

namespace loka
{
  namespace lrpc
  {
    namespace
    {
      bool DecodeCodepoint(const std::string &utf8,
                           std::size_t &index,
                           unsigned long &codepoint)
      {
        const unsigned char lead = static_cast<unsigned char>(utf8[index]);
        std::size_t continuationCount = 0;
        unsigned long minimum = 0;
        if (lead < 0x80)
        {
          codepoint = lead;
          ++index;
          return true;
        }
        if ((lead & 0xE0) == 0xC0)
        {
          continuationCount = 1;
          codepoint = lead & 0x1F;
          minimum = 0x80;
        }
        else if ((lead & 0xF0) == 0xE0)
        {
          continuationCount = 2;
          codepoint = lead & 0x0F;
          minimum = 0x800;
        }
        else if ((lead & 0xF8) == 0xF0)
        {
          continuationCount = 3;
          codepoint = lead & 0x07;
          minimum = 0x10000;
        }
        else
        {
          return false;
        }
        if (continuationCount > utf8.size() - index - 1)
        {
          return false;
        }
        for (std::size_t i = 1; i <= continuationCount; ++i)
        {
          const unsigned char continuation =
              static_cast<unsigned char>(utf8[index + i]);
          if ((continuation & 0xC0) != 0x80)
          {
            return false;
          }
          codepoint = (codepoint << 6) | (continuation & 0x3F);
        }
        if (codepoint < minimum || codepoint > 0x10FFFFUL ||
            (codepoint >= 0xD800UL && codepoint <= 0xDFFFUL))
        {
          return false;
        }
        index += continuationCount + 1;
        return true;
      }

      void AppendCodepoint(std::wstring &wide, unsigned long codepoint)
      {
#if WCHAR_MAX <= 0xFFFF
        if (codepoint > 0xFFFFUL)
        {
          codepoint -= 0x10000UL;
          wide.push_back(static_cast<wchar_t>(0xD800UL | (codepoint >> 10)));
          wide.push_back(static_cast<wchar_t>(0xDC00UL | (codepoint & 0x3FFUL)));
          return;
        }
#endif
        wide.push_back(static_cast<wchar_t>(codepoint));
      }
    } // namespace

    bool Utf8PathToWide(const std::string &utf8, std::wstring &out)
    {
      std::wstring converted;
      std::size_t index = 0;
      while (index < utf8.size())
      {
        unsigned long codepoint = 0;
        if (!DecodeCodepoint(utf8, index, codepoint))
        {
          return false;
        }
        AppendCodepoint(converted, codepoint);
      }
      out.swap(converted);
      return true;
    }
  } // namespace lrpc
} // namespace loka
