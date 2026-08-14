#ifndef LOKA_PLATFORM_FILE_FILEIO_HPP
#define LOKA_PLATFORM_FILE_FILEIO_HPP

#include <cstdio>

#include "core/String.hpp"
#include "platform/file/FileHandle.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      /**
       * Opens a file for reading, naming it the way the platform names files.
       *
       * This is a seam rather than a call to std::fopen because flattening a
       * logical String to UTF-8 and handing the bytes to a narrow open is
       * wrong wherever the narrow API is code-page based. Windows decodes the
       * argument of fopen in the process ANSI code page, so UTF-8 bytes for a
       * path containing full-width characters name a file that does not exist
       * (#15). Targets whose narrow API already takes bytes keep using fopen,
       * which AGENTS.md deliberately prefers on Classic paths.
       *
       * The caller owns the returned handle and must fclose it.
       *
       * @param path Logical path to open. Its native form is materialized by
       *             the platform implementation; no conversion happens here.
       * @return An open read-only handle, or NULL if the file cannot be opened.
       */
      std::FILE *OpenRead(const loka::core::String &path);

      /** Opens an already platform-resolved file destination for binary
          write. This preserves native location data such as a Classic FSSpec. */
      std::FILE *OpenWriteTruncate(const FileHandle &file);

      /** Commits one stdio record through the platform's durability boundary.
          The stream remains open; false means the caller must fail closed. */
      bool FlushWrite(std::FILE *stream, const FileHandle &file);
    } // namespace file
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_FILE_FILEIO_HPP
