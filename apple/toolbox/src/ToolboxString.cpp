#include "loka/platform/String.hpp"

#include <cstring>
#include <string>

namespace loka
{
  namespace platform
  {
    class ToolboxByteString : public String
    {
    public:
      ToolboxByteString() : data_() {}
      ToolboxByteString(const char *bytes, std::size_t length) : data_()
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

    loka::core::Managed<String> CreatePlatformStringFromUtf8(const char *bytes, std::size_t length)
    {
      ToolboxByteString *impl = new ToolboxByteString(bytes, length);
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
