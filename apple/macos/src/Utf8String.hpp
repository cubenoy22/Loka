#pragma once

#include <Foundation/Foundation.h>
#include <string>

namespace declara
{
  namespace macos
  {
  inline NSString *CreateNSStringFromUtf8(const std::string &value)
  {
    if (value.empty())
    {
      return @"";
    }
    NSString *string = [NSString stringWithUTF8String:value.c_str()];
    if (string)
    {
      return string;
    }
    NSData *data = [NSData dataWithBytes:value.data() length:value.size()];
    string = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
#if __has_feature(objc_arc)
    return string ? string : @"";
#else
    return string ? [string autorelease] : @"";
#endif
  }

  inline std::string Utf8FromNSString(NSString *string)
  {
    if (!string)
    {
      return std::string();
    }
    const char *bytes = [string UTF8String];
    if (!bytes)
    {
      return std::string();
    }
    return std::string(bytes);
  }
  }
}
