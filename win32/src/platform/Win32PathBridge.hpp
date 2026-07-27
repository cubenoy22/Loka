#ifndef LOKA_WIN32_PATH_BRIDGE_HPP
#define LOKA_WIN32_PATH_BRIDGE_HPP

#include <cstddef>

#include "core/io/File.hpp"
#include "core/String.hpp"
#include "platform/Win32String.hpp"

namespace loka
{
  namespace win32
  {
    /**
     * Builds the logical file item for a path Windows handed us as UTF-16.
     *
     * This exists as a named bridge rather than inline at the call site so the
     * conversion can be pinned: everything between "the OS gave us wide
     * characters" and "the framework holds a File" is ours, and it is where
     * #15 was lost. Building the same File from the `A` variant's bytes
     * instead sends them through `loka::core::String`, which reads them as
     * UTF-8, and a full-width path silently becomes different text.
     *
     * @param chars  UTF-16 path as returned by a `W` Win32 entry point.
     * @param length Number of UTF-16 code units, excluding any terminator.
     * @return A `File` naming that path, marked `KIND_FILE`.
     */
    inline loka::file::File FileFromWidePath(const wchar_t *chars, std::size_t length)
    {
      loka::file::File file =
          loka::file::File::FromPath(loka::core::String(CreateWin32StringFromUtf16(chars, length)));
      file.setKind(loka::file::File::KIND_FILE);
      return file;
    }
  } // namespace win32
} // namespace loka

#endif // LOKA_WIN32_PATH_BRIDGE_HPP
