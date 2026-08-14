#include "platform/file/AppLocation.hpp"

#include <Foundation/Foundation.h>

#include <cstring>
#include <string>

#include "platform/StringUTF8.hpp"

namespace
{
  bool
  ResolveFromDirectoryInPool(NSString *directory, const loka::file::File &item, loka::platform::file::FileHandle &out)
  {
    if (!directory)
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

    NSString *resolvedPath = [directory stringByAppendingPathComponent:relative];
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
    loka::platform::file::FileHandle completed;
    completed.displayPath = displayPath;
    completed.kind = item.kind();
    if (completed.displayPath.empty())
    {
      return false;
    }
    out = completed;
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
        const bool resolved = ResolveFromDirectoryInPool([[NSBundle mainBundle] resourcePath], item, out);
        [pool drain];
        return resolved;
      }

      bool ResolveApplicationSidecar(const loka::file::File &item, FileHandle &out)
      {
        out = FileHandle();
        if (!ApplicationRelativeIsOpenable(item))
        {
          return false;
        }
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSString *bundlePath = [[NSBundle mainBundle] bundlePath];
        NSString *directory = [bundlePath stringByDeletingLastPathComponent];
        NSFileManager *manager = [NSFileManager defaultManager];
        BOOL isDirectory = NO;
        const bool available = directory && [manager fileExistsAtPath:directory isDirectory:&isDirectory] && isDirectory
                               && [manager isWritableFileAtPath:directory];
        const bool resolved = available && ResolveFromDirectoryInPool(directory, item, out);
        [pool drain];
        return resolved;
      }
    } // namespace file
  } // namespace platform
} // namespace loka
