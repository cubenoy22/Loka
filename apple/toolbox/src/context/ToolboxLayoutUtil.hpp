#ifndef LOKA_TOOLBOX_LAYOUT_UTIL_HPP
#define LOKA_TOOLBOX_LAYOUT_UTIL_HPP

#include "loka/core/String.hpp"
#include "platform/StringUTF8.hpp"
#include <string>

static inline short ToolboxMeasureTextWidth(const loka::core::String &value)
{
  std::string utf8;
  if (!loka::platform::CollectUtf8(value, utf8))
  {
    return 40;
  }
  return static_cast<short>(utf8.size() * 7 + 16);
}

#endif // LOKA_TOOLBOX_LAYOUT_UTIL_HPP
