#ifndef LOKA_PLATFORM_STRING_UTF8_HPP
#define LOKA_PLATFORM_STRING_UTF8_HPP

#include <string>

#include "core/String.hpp"

namespace loka
{
  namespace platform
  {
    // Flattens a logical string into UTF-8 bytes. Intended for tooling/tests and JSON utilities.
    bool CollectUtf8(const core::String &source, std::string &out);
  }
}

#endif // LOKA_PLATFORM_STRING_UTF8_HPP
