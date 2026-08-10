#include "platform/String.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <cstring>
#include <vector>

namespace loka
{
  namespace platform
  {
    class CocoaUtf8String : public String
    {
    public:
      CocoaUtf8String(const char *bytes, std::size_t length)
          : value_(0)
      {
        if (bytes && length > 0)
        {
          value_ = CFStringCreateWithBytes(kCFAllocatorDefault,
                                           reinterpret_cast<const UInt8 *>(bytes),
                                           static_cast<CFIndex>(length),
                                           kCFStringEncodingUTF8,
                                           false);
        }
        if (!value_)
        {
          value_ = CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8);
        }
      }

      virtual ~CocoaUtf8String()
      {
        if (value_)
          CFRelease(value_);
      }

      virtual bool appendUtf8(std::string &out) const
      {
        if (!value_)
          return true;
        const CFIndex length = CFStringGetLength(value_);
        if (length == 0)
          return true;
        const CFIndex maxBytes = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
        if (maxBytes <= 0)
          return false;
        std::vector<char> buffer(static_cast<std::size_t>(maxBytes));
        CFIndex usedBytes = 0;
        const CFIndex converted =
            CFStringGetBytes(value_,
                             CFRangeMake(0, length),
                             kCFStringEncodingUTF8,
                             0,
                             false,
                             reinterpret_cast<UInt8 *>(&buffer[0]),
                             maxBytes,
                             &usedBytes);
        if (converted != length)
          return false;
        out.append(&buffer[0], static_cast<std::size_t>(usedBytes));
        return true;
      }

    private:
      CFStringRef value_;
    };

    loka::core::Managed<String> CreatePlatformStringFromUtf8(const char *bytes, std::size_t length)
    {
      CocoaUtf8String *impl = new CocoaUtf8String(bytes, length);
      return loka::core::Managed<String>::Wrap(impl);
    }

    loka::core::Managed<String> CreatePlatformStringFromLiteral(const char *literal)
    {
      if (!literal)
        return CreatePlatformStringFromUtf8("", 0);
      return CreatePlatformStringFromUtf8(literal, std::strlen(literal));
    }

  } // namespace platform
} // namespace loka
