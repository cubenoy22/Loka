#ifndef LOKA_TOOLBOX_PLATFORM_TOOLBOX_MAC_ROMAN_HPP
#define LOKA_TOOLBOX_PLATFORM_TOOLBOX_MAC_ROMAN_HPP

#include "core/String.hpp"
#include <cstddef>

namespace loka
{
  namespace toolbox
  {
    enum ToolboxMacRomanResult
    {
      TOOLBOX_MAC_ROMAN_COMPLETE,
      TOOLBOX_MAC_ROMAN_OUTPUT_FULL,
      TOOLBOX_MAC_ROMAN_INVALID
    };

    /** Encodes one logical string as MacRoman bytes into caller-owned storage. */
    ToolboxMacRomanResult CopyStringToMacRoman(const loka::core::String &value,
                                                unsigned char *out,
                                                std::size_t capacity,
                                                std::size_t &outLength);
  } // namespace toolbox
} // namespace loka

#endif // LOKA_TOOLBOX_PLATFORM_TOOLBOX_MAC_ROMAN_HPP
