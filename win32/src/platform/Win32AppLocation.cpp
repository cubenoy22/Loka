#include "platform/file/AppLocation.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

#include "platform/Win32String.hpp"

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

        DWORD capacity = MAX_PATH;
        DWORD moduleLength = 0;
        std::vector<wchar_t> moduleBuffer;
        for (;;)
        {
          moduleBuffer.resize(static_cast<std::size_t>(capacity));
          moduleLength = GetModuleFileNameW(NULL, &moduleBuffer[0], capacity);
          if (moduleLength == 0)
          {
            return false;
          }
          if (moduleLength < capacity)
          {
            break;
          }
          if (capacity > 32768)
          {
            return false;
          }
          capacity *= 2;
        }

        std::wstring resolvedPath(&moduleBuffer[0], &moduleBuffer[0] + moduleLength);
        const std::wstring::size_type separator = resolvedPath.find_last_of(L'\\');
        if (separator == std::wstring::npos)
        {
          return false;
        }
        resolvedPath.resize(separator);

        std::wstring relative;
        if (!loka::win32::MaterializeWideString(item.relativePath(), relative) || relative.empty())
        {
          return false;
        }
        resolvedPath += L'\\';
        resolvedPath += relative;

        const loka::core::String displayPath(
            loka::win32::CreateWin32StringFromUtf16(resolvedPath.c_str(), resolvedPath.length()));
        if (displayPath.empty())
        {
          return false;
        }
        out.displayPath = displayPath;
        out.kind = item.kind();
        return true;
      }

      bool ResolveApplicationSidecar(const loka::file::File &item, FileHandle &out)
      {
        return ResolveApplicationItem(item, out);
      }
    } // namespace file
  } // namespace platform
} // namespace loka
