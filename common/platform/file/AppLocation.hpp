#ifndef LOKA_PLATFORM_FILE_APP_LOCATION_HPP
#define LOKA_PLATFORM_FILE_APP_LOCATION_HPP

#include <string>

#include "core/io/File.hpp"
#include "platform/StringUTF8.hpp"
#include "platform/file/FileHandle.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      /**
       * Reports whether an application-relative item names exactly one file.
       *
       * Every platform shares this predicate so an application-relative File
       * has one portable meaning. Multi-segment paths remain future work.
       */
      inline bool ApplicationRelativeIsOpenable(const loka::file::File &item)
      {
        if (item.base() != loka::file::File::BASE_APPLICATION)
        {
          return false;
        }

        std::string relative;
        if (!loka::platform::CollectUtf8(item.relativePath(), relative))
        {
          return false;
        }
        return !relative.empty() && relative.find('/') == std::string::npos && relative.find('\\') == std::string::npos
               && relative.find(':') == std::string::npos && relative.find('\0') == std::string::npos && relative != "."
               && relative != "..";
      }

      /**
       * Resolves an application-relative item using the platform convention.
       *
       * @return False when the item is not application-relative, its relative
       *         name is refused, or the platform cannot locate the application.
       */
      bool ResolveApplicationItem(const loka::file::File &item, FileHandle &out);

      /** Resolves a writable peer of the application rather than packaged
          application content. The query declines when no peer is available. */
      bool ResolveApplicationSidecar(const loka::file::File &item, FileHandle &out);
    } // namespace file
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_FILE_APP_LOCATION_HPP
