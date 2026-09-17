#include "platform/ToolboxHfsName.hpp"

#include "platform/ToolboxMacRoman.hpp"

namespace loka
{
  namespace toolbox
  {
    bool CopyStringToHfsName(const loka::core::String &value, Str63 out)
    {
      out[0] = 0;
      std::size_t length = 0;
      if (CopyStringToMacRoman(value, out + 1, 31, length) != TOOLBOX_MAC_ROMAN_COMPLETE
          || length == 0)
        return false;
      out[0] = static_cast<unsigned char>(length);
      return true;
    }
  } // namespace toolbox
} // namespace loka
