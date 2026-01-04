#include "loka/core/String.hpp"

#include <string>

#include "loka/platform/String.hpp"
#include "loka/platform/StringUTF8.hpp"

namespace loka
{
  namespace core
  {

    String::String() : handle_()
    {
    }

    String::String(const char *literal) : handle_()
    {
      if (literal)
        handle_ = platform::CreatePlatformStringFromLiteral(literal);
    }

    String::String(const std::string &utf8) : handle_()
    {
      if (!utf8.empty())
        handle_ = platform::CreatePlatformStringFromUtf8(utf8.c_str(), utf8.size());
    }

    String::String(const Managed<platform::String> &handle) : handle_(handle)
    {
    }

    String::String(const String &other) : handle_(other.handle_)
    {
    }

    String &String::operator=(const String &other)
    {
      if (this != &other)
        handle_ = other.handle_;
      return *this;
    }

    String::~String()
    {
    }

    String String::Literal(const char *literal)
    {
      return String(literal);
    }

    String String::Utf8(const char *bytes, std::size_t length)
    {
      if (!bytes || length == 0)
        return String();
      return String(platform::CreatePlatformStringFromUtf8(bytes, length));
    }

    String String::FromPlatform(const Managed<platform::String> &platformValue)
    {
      return String(platformValue);
    }

    bool String::empty() const
    {
      return !handle_.isValid();
    }

    StringCompareResult String::compare(const String &other, bool allowAllocateBuffer) const
    {
      if (handle_ == other.handle_)
        return StringCompareEqual;
      if (!allowAllocateBuffer)
        return StringCompareBufferRequired;

      std::string left;
      if (!platform::CollectUtf8(*this, left))
        return StringCompareBufferRequired;

      std::string right;
      if (!platform::CollectUtf8(other, right))
        return StringCompareBufferRequired;

      if (left.size() != right.size())
        return StringCompareNotEqual;
      return left == right ? StringCompareEqual : StringCompareNotEqual;
    }

    bool String::equals(const String &other) const
    {
      return compare(other, true) == StringCompareEqual;
    }

    StringBuffer String::buffer() const
    {
      return bufferWithEncoding(StringEncodingUtf16);
    }

    StringBuffer String::bufferWithEncoding(StringEncoding encoding) const
    {
      StringBuffer buffer(encoding);
      buffer.setPlatformHandle(handle_);
      std::string utf8;
      if (!platform::CollectUtf8(*this, utf8))
        return StringBuffer(encoding);
      if (!buffer.assignFromUtf8(utf8))
        return StringBuffer(encoding);
      return buffer;
    }

    bool String::assignToBuffer(StringBuffer &buffer) const
    {
      buffer.setPlatformHandle(handle_);
      std::string utf8;
      if (!platform::CollectUtf8(*this, utf8))
        return false;
      return buffer.assignFromUtf8(utf8, false);
    }

    bool String::requiredUnits(StringEncoding encoding, std::size_t &units) const
    {
      std::string utf8;
      if (!platform::CollectUtf8(*this, utf8))
        return false;
      return StringBuffer::CountUnitsFromUtf8(utf8, encoding, units);
    }

    String String::operator+(const String &rhs) const
    {
      return Concat(*this, rhs);
    }

    String String::operator+(const char *rhs) const
    {
      return Concat(*this, String::Literal(rhs));
    }

    String String::operator+(const std::string &rhs) const
    {
      return Concat(*this, String(rhs));
    }

    String &String::operator+=(const String &rhs)
    {
      *this = Concat(*this, rhs);
      return *this;
    }

    String &String::operator+=(const char *rhs)
    {
      *this = Concat(*this, String::Literal(rhs));
      return *this;
    }

    String &String::operator+=(const std::string &rhs)
    {
      *this = Concat(*this, String(rhs));
      return *this;
    }

    String String::Concat(const String &lhs, const String &rhs)
    {
      if (lhs.empty())
        return rhs;
      if (rhs.empty())
        return lhs;
      std::string buffer;
      if (!platform::CollectUtf8(lhs, buffer))
        return String();
      if (!platform::CollectUtf8(rhs, buffer))
        return String();
      return String::Utf8(buffer.c_str(), buffer.size());
    }

    String String::FormatArray(const String &format, const String *args, std::size_t count)
    {
      std::string source;
      if (!platform::CollectUtf8(format, source))
        return String();

      std::string buffer;
      std::size_t segmentStart = 0;
      for (std::size_t i = 0; i < source.size(); ++i)
      {
        char ch = source[i];
        if (ch != '%')
          continue;
        if (i + 1 >= source.size())
          continue;
        if (i > segmentStart)
          buffer.append(source.c_str() + segmentStart, i - segmentStart);
        char next = source[i + 1];
        if (next == '%')
        {
          buffer.append("%", 1);
          segmentStart = i + 2;
          ++i;
          continue;
        }
        if (next >= '1' && next <= '9')
        {
          std::size_t index = static_cast<std::size_t>(next - '1');
          if (index < count)
          {
            if (!platform::CollectUtf8(args[index], buffer))
              return String();
          }
          segmentStart = i + 2;
          ++i;
          continue;
        }
        buffer.append("%", 1);
        segmentStart = i + 1;
      }
      if (segmentStart < source.size())
        buffer.append(source.c_str() + segmentStart, source.size() - segmentStart);
      return String::Utf8(buffer.c_str(), buffer.size());
    }

  } // namespace core
} // namespace loka
