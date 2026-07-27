// Byte-path implementation of the file open seam. Win32 supplies its own in
// win32/src/platform/Win32FileIO.cpp, mirroring how GenericString.cpp and
// Win32String.cpp split the platform string seam.
#if !defined(_WIN32)

#include "platform/file/FileIO.hpp"

#include <string>

#include "platform/StringUTF8.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      std::FILE *OpenRead(const loka::core::String &path)
      {
        std::string bytes;
        if (!loka::platform::CollectUtf8(path, bytes))
          return 0;
        if (bytes.empty())
          return 0;
        return std::fopen(bytes.c_str(), "rb");
      }
    } // namespace file
  } // namespace platform
} // namespace loka

#endif // !_WIN32
