#ifndef LOKA_PLATFORM_FILE_FILEHANDLE_HPP
#define LOKA_PLATFORM_FILE_FILEHANDLE_HPP

#include "core/io/File.hpp"
#include "core/String.hpp"
#if defined(LOKA_RETRO68)
#include <Files.h>
#endif

namespace loka
{
  namespace platform
  {
    namespace file
    {
      struct FileHandle
      {
        FileHandle()
            : displayPath(),
              kind(::loka::file::File::KIND_UNKNOWN)
#if defined(LOKA_RETRO68)
              ,
              hasSpec(false),
              spec()
#endif
        {
        }

        ::loka::core::String displayPath;
        ::loka::file::File::Kind kind;
#if defined(LOKA_RETRO68)
        bool hasSpec;
        FSSpec spec;
#endif
      };
    } // namespace file
  }   // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_FILE_FILEHANDLE_HPP
