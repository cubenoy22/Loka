#include "loka/platform/StringUTF8.hpp"
#include "loka/platform/String.hpp"
#include "loka/core/StringAccess.hpp"

namespace loka
{
  namespace platform
  {
    bool CollectUtf8(const core::String &source, std::string &out)
    {
      const loka::core::Managed<String> &handle = core::StringAccess::handle(source);
      if (!handle.isValid())
        return true;
      return handle->appendUtf8(out);
    }

  } // namespace platform
} // namespace loka
