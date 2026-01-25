#ifndef LOKA_PLATFORM_FILE_ITEMACCESS_HPP
#define LOKA_PLATFORM_FILE_ITEMACCESS_HPP

#include "file/Item.hpp"
#if defined(LOKA_RETRO68)
#include <Files.h>
#endif

namespace loka
{
  namespace platform
  {
    namespace file
    {
      struct ItemAccess
      {
        static ::loka::file::Item FromPath(const ::loka::core::String &value, ::loka::file::Item::Kind kind)
        {
          ::loka::file::Item item;
          item.base_ = ::loka::file::Item::BASE_PATH;
          item.basePath_ = value;
          item.kind_ = kind;
          return item;
        }

#if defined(LOKA_RETRO68)
      static ::loka::file::Item FromFSSpec(const FSSpec &value,
                                           ::loka::file::Item::Kind kind,
                                           const ::loka::core::String &displayPath)
      {
        ::loka::file::Item item;
        (void)value;
        item.base_ = ::loka::file::Item::BASE_PATH;
        item.basePath_ = displayPath;
        item.kind_ = kind;
        return item;
      }
#endif
      };
    } // namespace file
  }   // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_FILE_ITEMACCESS_HPP
