// Win32 half of the file open seam (see common/platform/file/FileIO.hpp).
// The generic byte-path implementation compiles out on _WIN32, exactly as
// GenericString.cpp does for the platform string seam.
#if defined(_WIN32)

#include "platform/file/FileIO.hpp"

#include <string>

#include "platform/Win32String.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      std::FILE *OpenRead(const loka::core::String &path)
      {
        // Every logical String on Win32 is UTF-16 backed (CreatePlatformStringFromUtf8
        // builds a Win32String), so materializing loses nothing. Going through
        // UTF-8 and fopen would hand the bytes to the ANSI code page instead.
        std::wstring wide;
        if (!loka::win32::MaterializeWideString(path, wide))
          return 0;
        if (wide.empty())
          return 0;
        return _wfopen(wide.c_str(), L"rb");
      }
    } // namespace file
  } // namespace platform
} // namespace loka

#endif // _WIN32
