#ifndef LOKA_PLATFORM_FILE_FILEIO_HPP
#define LOKA_PLATFORM_FILE_FILEIO_HPP

#include <cstdio>

#include "core/String.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      // Opens a file for reading, naming it the way the platform names files.
      //
      // The reason this is a seam rather than a call to std::fopen: flattening
      // a logical String to UTF-8 and handing the bytes to a narrow open is
      // wrong wherever the narrow API is code-page based. Windows decodes the
      // argument of fopen in the process ANSI code page, so UTF-8 bytes for a
      // path containing full-width characters name a file that does not exist
      // (#15). Targets whose narrow API already takes bytes keep using fopen,
      // which AGENTS.md deliberately prefers on Classic paths.
      //
      // Returns NULL on failure. The caller owns the returned handle.
      std::FILE *OpenRead(const loka::core::String &path);
    } // namespace file
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_FILE_FILEIO_HPP
