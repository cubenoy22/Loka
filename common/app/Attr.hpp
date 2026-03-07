#ifndef LOKA_APP_ATTR_HPP
#define LOKA_APP_ATTR_HPP

#include "loka/core/State.hpp"

namespace loka
{
  namespace app
  {
    // Common base attr kept intentionally small.
    // View-specific attrs (Text/ImageView/MenuItem, etc.) must live next to each view definition.
    struct CommonAttr
    {
      CommonAttr() {}
      bool operator==(const CommonAttr &) const { return true; }
      bool operator<(const CommonAttr &) const { return false; }
    };
  } // namespace app
} // namespace loka

#endif // LOKA_APP_ATTR_HPP
