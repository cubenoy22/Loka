#include "loka/platform/StringUTF8.hpp"
#include "loka/platform/String.hpp"

namespace loka
{
  namespace platform
  {
    bool CollectUtf8(const core::String &source, std::string &out)
    {
      const Managed<String> &handle = source.handle();
      if (!handle.isValid())
        return true;
      return handle->appendUtf8(out);
    }

  } // namespace platform
} // namespace loka
