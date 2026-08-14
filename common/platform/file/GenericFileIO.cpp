// Byte-path implementation of the file open seam. Win32 supplies its own in
// win32/src/platform/Win32FileIO.cpp, mirroring how GenericString.cpp and
// Win32String.cpp split the platform string seam.
#if !defined(_WIN32)

#include "platform/file/FileIO.hpp"

#include <string>
#include <unistd.h>

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

#if !defined(LOKA_RETRO68)
      std::FILE *OpenWriteTruncate(const FileHandle &file)
      {
        std::string bytes;
        if (!loka::platform::CollectUtf8(file.displayPath, bytes))
          return 0;
        if (bytes.empty())
          return 0;
        return std::fopen(bytes.c_str(), "wb");
      }

      bool FlushWrite(std::FILE *stream, const FileHandle &)
      {
        if (stream == 0 || std::fflush(stream) != 0)
          return false;
        const int descriptor = fileno(stream);
        return descriptor >= 0 && fsync(descriptor) == 0;
      }
#endif
    } // namespace file
  } // namespace platform
} // namespace loka

#endif // !_WIN32
