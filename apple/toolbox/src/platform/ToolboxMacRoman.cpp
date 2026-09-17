#include "ToolboxMacRoman.hpp"

#include "platform/StringUTF8.hpp"
#include <string>

namespace loka
{
  namespace toolbox
  {
    ToolboxMacRomanResult CopyStringToMacRoman(const loka::core::String &value,
                                                unsigned char *out,
                                                std::size_t capacity,
                                                std::size_t &outLength)
    {
      // MacRoman byte 0xDB maps to U+00A4 deliberately (pre-euro table, System 7 semantics).
      static const unsigned int kMacRomanCodepoints[128] = {
          0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1, 0x00E0, 0x00E2, 0x00E4, 0x00E3,
          0x00E5, 0x00E7, 0x00E9, 0x00E8, 0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,
          0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC, 0x2020, 0x00B0, 0x00A2, 0x00A3,
          0x00A7, 0x2022, 0x00B6, 0x00DF, 0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
          0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211, 0x220F, 0x03C0, 0x222B, 0x00AA,
          0x00BA, 0x03A9, 0x00E6, 0x00F8, 0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,
          0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153, 0x2013, 0x2014, 0x201C, 0x201D,
          0x2018, 0x2019, 0x00F7, 0x25CA, 0x00FF, 0x0178, 0x2044, 0x00A4, 0x2039, 0x203A, 0xFB01, 0xFB02,
          0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1, 0x00CB, 0x00C8, 0x00CD, 0x00CE,
          0x00CF, 0x00CC, 0x00D3, 0x00D4, 0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,
          0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7};

      outLength = 0;
      if (!out && capacity > 0)
      {
        return TOOLBOX_MAC_ROMAN_INVALID;
      }
      std::string utf8;
      if (!loka::platform::CollectUtf8(value, utf8))
      {
        return TOOLBOX_MAC_ROMAN_INVALID;
      }

      std::size_t input = 0;
      while (input < utf8.size())
      {
        if (outLength >= capacity)
        {
          return TOOLBOX_MAC_ROMAN_OUTPUT_FULL;
        }
        const unsigned char first = static_cast<unsigned char>(utf8[input]);
        unsigned int codepoint = 0;
        std::size_t continuationCount = 0;
        unsigned int minimum = 0;
        if (first < 0x80)
        {
          codepoint = first;
        }
        else if ((first & 0xE0) == 0xC0)
        {
          codepoint = first & 0x1F;
          continuationCount = 1;
          minimum = 0x80;
        }
        else if ((first & 0xF0) == 0xE0)
        {
          codepoint = first & 0x0F;
          continuationCount = 2;
          minimum = 0x800;
        }
        else if ((first & 0xF8) == 0xF0)
        {
          codepoint = first & 0x07;
          continuationCount = 3;
          minimum = 0x10000;
        }
        else
        {
          return TOOLBOX_MAC_ROMAN_INVALID;
        }

        if (input + continuationCount >= utf8.size())
        {
          return TOOLBOX_MAC_ROMAN_INVALID;
        }
        for (std::size_t i = 0; i < continuationCount; ++i)
        {
          const unsigned char next = static_cast<unsigned char>(utf8[input + i + 1]);
          if ((next & 0xC0) != 0x80)
          {
            return TOOLBOX_MAC_ROMAN_INVALID;
          }
          codepoint = (codepoint << 6) | (next & 0x3F);
        }
        if ((continuationCount > 0 && codepoint < minimum) || codepoint > 0x10FFFF
            || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
        {
          return TOOLBOX_MAC_ROMAN_INVALID;
        }
        input += continuationCount + 1;

        unsigned int macRomanByte = 0;
        bool representable = false;
        if (codepoint < 0x80)
        {
          macRomanByte = codepoint;
          representable = true;
        }
        else
        {
          for (unsigned int i = 0; i < 128; ++i)
          {
            if (kMacRomanCodepoints[i] == codepoint)
            {
              macRomanByte = i + 0x80;
              representable = true;
              break;
            }
          }
        }
        if (!representable)
        {
          return TOOLBOX_MAC_ROMAN_INVALID;
        }
        out[outLength] = static_cast<unsigned char>(macRomanByte);
        ++outLength;
      }
      return TOOLBOX_MAC_ROMAN_COMPLETE;
    }
  } // namespace toolbox
} // namespace loka
