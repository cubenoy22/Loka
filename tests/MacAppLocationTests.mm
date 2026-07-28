#include "MacAppLocationTests.hpp"

#include <Foundation/Foundation.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileIO.hpp"

void testMacApplicationItemNamesResourceDirectory()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSString *executablePath = [[NSBundle mainBundle] executablePath];
  NSString *executableDirectory = [executablePath stringByDeletingLastPathComponent];
  NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
  assert(executablePath);
  assert(executableDirectory);
  assert([resourcePath isEqualToString:executableDirectory]);

  NSString *name = @"loka-application-location.bin";
  NSString *fixturePath = [executableDirectory stringByAppendingPathComponent:name];
  const unsigned char expected[] = {0x19, 0x9A, 0x02};
  NSData *payload = [NSData dataWithBytes:expected length:sizeof(expected)];
  assert([payload writeToFile:fixturePath atomically:NO]);

  const char *nameUtf8 = [name UTF8String];
  assert(nameUtf8);
  const loka::file::File item = loka::file::File::Application()
                                << loka::file::File(loka::core::String::Utf8(nameUtf8, std::strlen(nameUtf8)));
  loka::platform::file::FileHandle handle;
  assert(loka::platform::file::ResolveApplicationItem(item, handle));

  const char *pathUtf8 = [fixturePath UTF8String];
  assert(pathUtf8);
  assert(handle.displayPath.equals(loka::core::String::Utf8(pathUtf8, std::strlen(pathUtf8))));

  std::FILE *opened = loka::platform::file::OpenRead(handle.displayPath);
  assert(opened);
  unsigned char actual[sizeof(expected)] = {0};
  assert(std::fread(actual, 1, sizeof(actual), opened) == sizeof(actual));
  assert(std::fclose(opened) == 0);
  assert(std::memcmp(actual, expected, sizeof(actual)) == 0);

  assert([[NSFileManager defaultManager] removeItemAtPath:fixturePath error:nil]);
  [pool drain];
}
