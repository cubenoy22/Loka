#if !defined(_WIN32) && !defined(__APPLE__) && !defined(LOKA_RETRO68)

#include "platform/file/AppLocation.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      bool ResolveApplicationItem(const loka::file::File &item, FileHandle &out)
      {
        out = FileHandle();
        if (!ApplicationRelativeIsOpenable(item))
        {
          return false;
        }
        return false;
      }
    } // namespace file
  } // namespace platform
} // namespace loka

#endif
