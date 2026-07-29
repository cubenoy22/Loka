#include "core/resource/lrpk/LrpkStdioByteSource.hpp"

#include <climits>

#include "platform/file/FileIO.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      namespace lrpk
      {
        StdioByteSource::StdioByteSource()
            : file_(0)
        {
        }

        StdioByteSource::~StdioByteSource()
        {
          close();
        }

        bool StdioByteSource::open(const loka::core::String &path)
        {
          close();
          file_ = loka::platform::file::OpenRead(path);
          return file_ != 0;
        }

        void StdioByteSource::close()
        {
          if (file_)
          {
            std::fclose(file_);
            file_ = 0;
          }
        }

        bool StdioByteSource::readAt(std::size_t at,
                                     unsigned char *dst,
                                     std::size_t n)
        {
          if (!file_)
          {
            return false;
          }
          if (n == 0)
          {
            // Nothing to deliver, so nothing can fail -- and no seek, which
            // keeps a zero-length bag from being refused by a position the
            // stream cannot express.
            return true;
          }
          if (!dst)
          {
            return false;
          }
          // fseek addresses with a signed long. A position it cannot express
          // is a transport limit, reported as such rather than clamped into a
          // read of the wrong bytes.
          if (at > static_cast<std::size_t>(LONG_MAX))
          {
            return false;
          }
          if (std::fseek(file_, static_cast<long>(at), SEEK_SET) != 0)
          {
            return false;
          }
          // Complete read or nothing: the interface has no partial answer, so
          // a short count is a failure here rather than a number the caller
          // has to loop on.
          return std::fread(dst, 1, n, file_) == n;
        }

        bool StdioByteSource::size(std::size_t &out)
        {
          out = 0;
          if (!file_)
          {
            return false;
          }
          if (std::fseek(file_, 0, SEEK_END) != 0)
          {
            return false;
          }
          const long end = std::ftell(file_);
          if (end < 0)
          {
            return false;
          }
          out = static_cast<std::size_t>(end);
          return true;
        }
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka
