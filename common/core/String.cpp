#include "core/String.hpp"

#include <string>
#include <cstdio>
#include <cstring>

#include "platform/String.hpp"
#include "platform/StringUTF8.hpp"

namespace loka
{
  namespace core
  {
    // Lazy concatenation - stores references instead of flattening
    class ConcatString : public platform::String
    {
    public:
      ConcatString(const loka::core::Managed<platform::String> &left,
                   const loka::core::Managed<platform::String> &right)
          : left_(left),
            right_(right)
      {
      }

      virtual bool appendUtf8(std::string &out) const
      {
        if (left_.isValid() && !left_->appendUtf8(out))
          return false;
        if (right_.isValid() && !right_->appendUtf8(out))
          return false;
        return true;
      }

    private:
      loka::core::Managed<platform::String> left_;
      loka::core::Managed<platform::String> right_;
    };

    String::String()
        : handle_()
    {
    }

    String::String(const char *literal)
        : handle_()
    {
      if (literal)
        handle_ = platform::CreatePlatformStringFromLiteral(literal);
    }

    String::String(const std::string &utf8)
        : handle_()
    {
      if (!utf8.empty())
        handle_ = platform::CreatePlatformStringFromUtf8(utf8.c_str(), utf8.size());
    }

    String::String(const loka::core::Managed<platform::String> &handle)
        : handle_(handle)
    {
    }

    String::String(const String &other)
        : handle_(other.handle_)
    {
    }

    String &String::operator=(const String &other)
    {
      if (this != &other)
        handle_ = other.handle_;
      return *this;
    }

    String::~String() {}

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

    String String::FromPlatform(const loka::core::Managed<platform::String> &platformValue)
    {
      return String(platformValue);
    }

    String String::FromInt(int value)
    {
      char buf[32];
      ::snprintf(buf, sizeof(buf), "%d", value);
      return String::Utf8(buf, std::strlen(buf));
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

    int String::compare(const String &other) const
    {
      if (handle_ == other.handle_)
      {
        return 0;
      }

      StringBuffer leftBuffer = bufferWithEncoding(StringEncodingUtf8);
      StringBuffer rightBuffer = other.bufferWithEncoding(StringEncodingUtf8);
      std::size_t leftLength = leftBuffer.length();
      std::size_t rightLength = rightBuffer.length();
      std::size_t commonLength = leftLength < rightLength ? leftLength : rightLength;
      if (commonLength > 0)
      {
        int compareResult = std::memcmp(leftBuffer.data(), rightBuffer.data(), commonLength);
        if (compareResult < 0)
        {
          return -1;
        }
        if (compareResult > 0)
        {
          return 1;
        }
      }
      if (leftLength < rightLength)
      {
        return -1;
      }
      if (rightLength < leftLength)
      {
        return 1;
      }
      return 0;
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
      // Lazy concatenation - don't flatten until needed
      return String(loka::core::Managed<platform::String>::Wrap(new ConcatString(lhs.handle_, rhs.handle_)));
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
