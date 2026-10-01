#include "platform/file/FileIO.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      ReadResult ReadBytes(const loka::core::String &path,
                           std::vector<unsigned char> &out,
                           const ReadCapacity *capacity)
      {
        out.clear();
        FILE *file = loka::platform::file::OpenRead(path);
        if (!file)
        {
          return READ_STDIO_OPEN_FAILED;
        }

        if (std::fseek(file, 0, SEEK_END) == 0)
        {
          long length = std::ftell(file);
          if (length >= 0)
          {
            if (capacity && !capacity->allows(static_cast<std::size_t>(length)))
            {
              std::fclose(file);
              return READ_CAPACITY_REFUSED;
            }
            out.resize(static_cast<std::size_t>(length));
            if (std::fseek(file, 0, SEEK_SET) != 0)
            {
              std::fclose(file);
              out.clear();
              return READ_STDIO_SEEK_FAILED;
            }
            if (!out.empty())
            {
              std::size_t readBytes = std::fread(&out[0], 1, out.size(), file);
              if (readBytes != out.size())
              {
                if (std::ferror(file))
                {
                  std::fclose(file);
                  out.clear();
                  return READ_STDIO_READ_FAILED;
                }
                out.resize(readBytes);
              }
            }
            std::fclose(file);
            return READ_OK;
          }
        }

        if (std::fseek(file, 0, SEEK_SET) != 0)
        {
          std::fclose(file);
          return READ_STDIO_SEEK_FAILED;
        }
        const std::size_t kBufferSize = 4096;
        unsigned char buffer[kBufferSize];
        for (;;)
        {
          std::size_t readBytes = std::fread(buffer, 1, kBufferSize, file);
          if (readBytes > 0)
          {
            std::size_t oldSize = out.size();
            if (readBytes > static_cast<std::size_t>(-1) - oldSize)
            {
              std::fclose(file);
              out.clear();
              return READ_SIZE_OVERFLOW;
            }
            if (capacity && !capacity->allows(oldSize + readBytes))
            {
              std::fclose(file);
              out.clear();
              return READ_CAPACITY_REFUSED;
            }
            out.resize(oldSize + readBytes);
            for (std::size_t i = 0; i < readBytes; ++i)
            {
              out[oldSize + i] = buffer[i];
            }
          }
          if (readBytes < kBufferSize)
          {
            if (std::ferror(file))
            {
              std::fclose(file);
              out.clear();
              return READ_STDIO_READ_FAILED;
            }
            break;
          }
        }

        std::fclose(file);
        return READ_OK;
      }
    } // namespace file
  } // namespace platform
} // namespace loka
