#include "platform/ToolboxPascalText.hpp"
#include "platform/ToolboxMacRoman.hpp"
#include "platform/StringUTF8.hpp"
#include <Script.h>
#include <cstring>

namespace
{
  // Validation mirrors strict HFS decoding in ToolboxHfsName.cpp; recovery is
  // deliberately different: the caller consumes only one byte on refusal.
  bool DecodeScalar(const char *utf8, std::size_t remaining,
                    unsigned long &scalar, std::size_t &length)
  {
    const unsigned char first = static_cast<unsigned char>(utf8[0]);
    unsigned long minimum = 0;
    if (first < 0x80) { scalar = first; length = 1; }
    else if ((first & 0xE0) == 0xC0) { scalar = first & 0x1F; length = 2; minimum = 0x80; }
    else if ((first & 0xF0) == 0xE0) { scalar = first & 0x0F; length = 3; minimum = 0x800; }
    else if ((first & 0xF8) == 0xF0) { scalar = first & 0x07; length = 4; minimum = 0x10000; }
    else return false;
    if (length > remaining)
      return false;
    for (std::size_t i = 1; i < length; ++i)
    {
      const unsigned char next = static_cast<unsigned char>(utf8[i]);
      if ((next & 0xC0) != 0x80)
        return false;
      scalar = (scalar << 6) | (next & 0x3F);
    }
    return scalar >= minimum && scalar <= 0x10FFFF
        && !(scalar >= 0xD800 && scalar <= 0xDFFF);
  }
}

ToolboxTextUnit ToolboxNextTextUnit(const char *input, std::size_t remaining, bool roman)
{
  assert(input && remaining);
  ToolboxTextUnit unit = { '?', 1, '?' };
  unsigned long scalar = 0;
  std::size_t length = 1;
  if (DecodeScalar(input, remaining, scalar, length))
  {
    unit.scalar = scalar;
    unit.consumed = length;
    if (scalar < 0x80)
      unit.native = static_cast<unsigned char>(scalar);
    else if (roman)
      ToolboxMacRomanEncode(scalar, unit.native);
  }
  return unit;
}

void ToolboxNativeText::clear()
{
  this->bytes_.clear();
  this->source_ = loka::core::String();
}

bool ToolboxNativeText::build(const loka::core::String &value)
{
  this->clear();
  const bool roman = GetScriptManagerVariable(smSysScript) == smRoman;
  std::string utf8;
  if (!loka::platform::CollectUtf8(value, utf8))
    return false;
  std::size_t count = 0;
  for (std::size_t input = 0; input < utf8.size(); ++count)
    input += ToolboxNextTextUnit(utf8.data() + input, utf8.size() - input, roman).consumed;
  if (!this->bytes_.allocate(count))
    return false;
  count = 0;
  for (std::size_t input = 0; input < utf8.size();)
  {
    const ToolboxTextUnit unit = ToolboxNextTextUnit(utf8.data() + input, utf8.size() - input, roman);
    this->bytes_[count++] = unit.native;
    input += unit.consumed;
  }
  this->source_ = value;
  return true;
}

std::size_t ToolboxNativeText::next(std::size_t offset) const
{
  assert(this->valid() && offset < this->size());
  // T1 emits one byte per strict unit. T2 supplies variable boundaries here.
  return offset + 1;
}

std::size_t ToolboxNativeText::previous(std::size_t end) const
{
  assert(this->valid() && end > 0 && end <= this->size());
  return end - 1;
}

std::size_t ToolboxNativeText::cappedEnd(std::size_t budget) const
{
  std::size_t end = 0;
  while (end < this->size())
  {
    const std::size_t next = this->next(end);
    if (next > budget)
      break;
    end = next;
  }
  return end;
}

void ToolboxNativeText::copyPascal(std::size_t start, std::size_t end, Str255 out) const
{
  assert(this->valid() && start <= end && end <= this->size() && end - start <= 255);
  out[0] = static_cast<unsigned char>(end - start);
  if (end != start)
    std::memcpy(out + 1, this->data() + start, end - start);
}

bool ToolboxEncodePascal(const loka::core::String &value, Str255 out)
{
  out[0] = 0;
  const bool roman = GetScriptManagerVariable(smSysScript) == smRoman;
  std::string utf8;
  if (!loka::platform::CollectUtf8(value, utf8))
    return false;
  // Stream straight into the capped output: building a full projection here
  // would add an allocation refusal that no short label had before (T1a).
  std::size_t count = 0;
  for (std::size_t input = 0; input < utf8.size() && count < 255;)
  {
    const ToolboxTextUnit unit = ToolboxNextTextUnit(utf8.data() + input, utf8.size() - input, roman);
    out[++count] = unit.native;
    input += unit.consumed;
  }
  out[0] = static_cast<unsigned char>(count);
  return true;
}
