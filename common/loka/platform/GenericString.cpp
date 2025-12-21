#ifndef _WIN32

#include "loka/platform/String.hpp"

#include <cstring>

namespace loka
{
  namespace platform
  {
    class GenericUtf8String : public String
    {
    public:
      GenericUtf8String() : data_() {}
      GenericUtf8String(const char *bytes, std::size_t length) : data_()
      {
        if (bytes && length > 0)
          data_.assign(bytes, length);
      }

      virtual bool appendUtf8(std::string &out) const
      {
        out.append(data_);
        return true;
      }

    private:
      std::string data_;
    };

    Managed<String> CreatePlatformStringFromUtf8(const char *bytes, std::size_t length)
    {
      GenericUtf8String *impl = new GenericUtf8String(bytes, length);
      return Managed<String>::Wrap(impl);
    }

    Managed<String> CreatePlatformStringFromLiteral(const char *literal)
    {
      if (!literal)
        return CreatePlatformStringFromUtf8("", 0);
      return CreatePlatformStringFromUtf8(literal, std::strlen(literal));
    }

  } // namespace platform
} // namespace loka

#endif // !_WIN32
