#include "platform/file/FileIO.hpp"
#include <cstring>

namespace loka
{
  namespace platform
  {
    namespace file
    {
      namespace
      {
        /** Owns the stream for the entire synchronous read. */
        class ReadStream
        {
        public:
          explicit ReadStream(FILE *file)
              : file_(file)
          {
          }
          ~ReadStream()
          {
            if (this->file_)
              std::fclose(this->file_);
          }
          FILE *get() const
          {
            return this->file_;
          }

        private:
          FILE *file_;
          ReadStream(const ReadStream &);
          ReadStream &operator=(const ReadStream &);
        };

        ReadResult Fill(FILE *file, core::resource::Blob &out, const ReadCapacity *capacity)
        {
          if (std::fseek(file, 0, SEEK_END) == 0)
          {
            const long length = std::ftell(file);
            if (length >= 0)
            {
              if (capacity && !capacity->allows(static_cast<std::size_t>(length)))
                return READ_CAPACITY_REFUSED;
              if (!out.tryResize(static_cast<std::size_t>(length)))
                return READ_ALLOCATION_REFUSED;
              if (std::fseek(file, 0, SEEK_SET) != 0)
                return READ_STDIO_SEEK_FAILED;
              if (out.size())
              {
                const std::size_t received = std::fread(out.mutableData(), 1, out.size(), file);
                if (std::ferror(file))
                  return READ_STDIO_READ_FAILED;
                out.tryResize(received);
              }
              return READ_OK;
            }
          }
          if (std::fseek(file, 0, SEEK_SET) != 0)
            return READ_STDIO_SEEK_FAILED;
          unsigned char buffer[4096];
          std::size_t filled = 0;
          for (;;)
          {
            const std::size_t received = std::fread(buffer, 1, sizeof(buffer), file);
            if (received)
            {
              const std::size_t maximum = static_cast<std::size_t>(-1);
              if (received > maximum - filled)
                return READ_SIZE_OVERFLOW;
              const std::size_t required = filled + received;
              if (capacity && !capacity->allows(required))
                return READ_CAPACITY_REFUSED;
              if (required > out.size())
              {
                std::size_t extent = out.size() ? out.size() : sizeof(buffer);
                while (extent < required)
                  extent = extent > maximum / 2 ? required : extent * 2;
                if (!out.tryResize(extent))
                  return READ_ALLOCATION_REFUSED;
              }
              std::memcpy(out.mutableData() + filled, buffer, received);
              filled = required;
            }
            if (received < sizeof(buffer))
            {
              if (std::ferror(file))
                return READ_STDIO_READ_FAILED;
              break;
            }
          }
          out.tryResize(filled);
          return READ_OK;
        }
      } // namespace

      ReadResult
      ReadBytesThroughStdio(const core::String &path, core::resource::Blob &out, const ReadCapacity *capacity)
      {
        if (!out.isValid() || out.isCompleted())
          return READ_ALLOCATION_REFUSED;
        out.tryResize(0);
        const ReadStream stream(OpenRead(path));
        if (!stream.get())
          return READ_STDIO_OPEN_FAILED;
        const ReadResult result = Fill(stream.get(), out, capacity);
        if (result != READ_OK)
          out.tryResize(0);
        return result;
      }
    } // namespace file
  } // namespace platform
} // namespace loka
