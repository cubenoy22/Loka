#include "platform/file/AppLocation.hpp"

#include <Foundation/Foundation.h>

#include <cstring>
#include <string>

#include "platform/StringUTF8.hpp"

namespace
{
  bool ResolveApplicationItemInPool(const loka::file::File &item,
                                    loka::platform::file::FileHandle &out)
  {
    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    if (!resourcePath)
    {
      return false;
    }

    std::string relativeBytes;
    if (!loka::platform::CollectUtf8(item.relativePath(), relativeBytes) || relativeBytes.empty())
    {
      return false;
    }
    NSString *relative = [[[NSString alloc] initWithBytes:relativeBytes.data()
                                                   length:relativeBytes.size()
                                                 encoding:NSUTF8StringEncoding] autorelease];
    if (!relative)
    {
      return false;
    }

    NSString *resolvedPath = [[resourcePath stringByAppendingString:@"/"] stringByAppendingString:relative];
    const char *utf8 = [resolvedPath UTF8String];
    if (!utf8)
    {
      return false;
    }

    const loka::core::String displayPath = loka::core::String::Utf8(utf8, std::strlen(utf8));
    if (displayPath.empty())
    {
      return false;
    }
    out.displayPath = displayPath;
    out.kind = item.kind();
    return true;
  }
} // namespace

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

        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        const bool resolved = ResolveApplicationItemInPool(item, out);
        [pool drain];
        return resolved;
      }
    } // namespace file
  } // namespace platform
} // namespace loka
