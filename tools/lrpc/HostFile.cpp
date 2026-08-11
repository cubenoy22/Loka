#include "lrpc/HostFile.hpp"

#include <cstdio>

namespace loka
{
  namespace lrpc
  {
    namespace
    {
      bool ReadOpenFile(std::FILE *file, std::vector<unsigned char> &out)
      {
        if (!file)
        {
          return false;
        }
        out.clear();
        unsigned char chunk[4096];
        for (;;)
        {
          const std::size_t got = std::fread(chunk, 1, sizeof(chunk), file);
          if (got > 0)
          {
            out.insert(out.end(), chunk, chunk + got);
          }
          if (got < sizeof(chunk))
          {
            break;
          }
        }
        const bool ok = std::ferror(file) == 0;
        std::fclose(file);
        return ok;
      }
    } // namespace

    bool ReadWholeFile(const std::string &path, std::vector<unsigned char> &out)
    {
      return ReadOpenFile(std::fopen(path.c_str(), "rb"), out);
    }

#if defined(_WIN32)
    bool ReadWholeFile(const std::wstring &path, std::vector<unsigned char> &out)
    {
      return ReadOpenFile(_wfopen(path.c_str(), L"rb"), out);
    }
#endif
  } // namespace lrpc
} // namespace loka
