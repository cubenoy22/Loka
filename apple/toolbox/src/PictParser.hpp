#ifndef LOKA_APPLE_TOOLBOX_PICTPARSER_HPP
#define LOKA_APPLE_TOOLBOX_PICTPARSER_HPP

#include <cstddef>
#include <vector>

namespace loka
{
  namespace toolbox
  {
    namespace pict
    {
      /** The bounded stream and frame selected from a PICT byte range. */
      struct PictParseResult
      {
        PictParseResult();

        std::size_t pictureOffset;
        std::size_t pictureSize;
        int width;
        int height;
      };

      /** Selects a versioned raw stream or one after a 512-byte file header. */
      bool ParsePict(const std::vector<unsigned char> &bytes,
                     std::size_t base,
                     std::size_t limit,
                     PictParseResult &out);
    }
  }
}

#endif // LOKA_APPLE_TOOLBOX_PICTPARSER_HPP
