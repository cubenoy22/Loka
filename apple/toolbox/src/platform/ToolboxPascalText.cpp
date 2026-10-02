#include "platform/ToolboxPascalText.hpp"
#include "platform/ToolboxMacRoman.hpp"
#include "platform/StringUTF8.hpp"
#include <Script.h>

namespace
{
  // Validation mirrors strict HFS decoding in ToolboxHfsName.cpp; recovery is
  // deliberately different: the caller consumes only one byte on refusal.
  bool DecodeScalar(const std::string &utf8, std::size_t input,
                    unsigned long &scalar, std::size_t &length)
  {
    const unsigned char first = static_cast<unsigned char>(utf8[input]);
    unsigned long minimum = 0;
    if (first < 0x80) { scalar = first; length = 1; }
    else if ((first & 0xE0) == 0xC0) { scalar = first & 0x1F; length = 2; minimum = 0x80; }
    else if ((first & 0xF0) == 0xE0) { scalar = first & 0x0F; length = 3; minimum = 0x800; }
    else if ((first & 0xF8) == 0xF0) { scalar = first & 0x07; length = 4; minimum = 0x10000; }
    else return false;
    if (length > utf8.size() - input)
      return false;
    for (std::size_t i = 1; i < length; ++i)
    {
      const unsigned char next = static_cast<unsigned char>(utf8[input + i]);
      if ((next & 0xC0) != 0x80)
        return false;
      scalar = (scalar << 6) | (next & 0x3F);
    }
    return scalar >= minimum && scalar <= 0x10FFFF
        && !(scalar >= 0xD800 && scalar <= 0xDFFF);
  }
}

bool ToolboxEncodePascal(const loka::core::String &value, Str255 out)
{
  const bool roman = GetScriptManagerVariable(smSysScript) == smRoman;
  out[0] = 0;
  std::string utf8;
  if (!loka::platform::CollectUtf8(value, utf8))
    return false;
  std::size_t input = 0;
  unsigned int count = 0;
  while (input < utf8.size() && count < 255)
  {
    unsigned long scalar = 0;
    std::size_t length = 1;
    unsigned char byte = '?';
    if (DecodeScalar(utf8, input, scalar, length))
    {
      if (scalar < 0x80)
        byte = static_cast<unsigned char>(scalar);
      else if (roman)
        ToolboxMacRomanEncode(scalar, byte);
      input += length;
    }
    else
      ++input;
    out[++count] = byte;
  }
  out[0] = static_cast<unsigned char>(count);
  return true;
}
