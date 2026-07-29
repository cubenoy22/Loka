#ifndef LOKA_CORE_RESOURCE_BLOBRANGE_HPP
#define LOKA_CORE_RESOURCE_BLOBRANGE_HPP

#include <cstddef>

namespace loka
{
  namespace core
  {
    namespace resource
    {
      /**
       * Whether `[offset, offset + length)` is a usable range inside a buffer
       * of `size` bytes.
       *
       * Shared by every `createImageFromBlob` implementation so the four of
       * them cannot disagree, and separated from them so it can be pinned
       * headlessly -- the null platform answers `false` to every image
       * request, so a range's acceptance is not observable through it.
       *
       * The comparison avoids forming `offset + length`, which wraps: a caller
       * passing `(4, SIZE_MAX)` would otherwise compute a small sum and pass a
       * check it should fail. Subtracting instead keeps every term inside the
       * buffer's own magnitude.
       *
       * An empty range is refused rather than treated as an empty image. There
       * is no such thing as a zero-byte asset to decode, and accepting one only
       * moves the failure to whichever decoder indexes the first byte.
       */
      inline bool BlobRangeIsUsable(std::size_t size, std::size_t offset, std::size_t length)
      {
        return length != 0 && offset <= size && length <= size - offset;
      }
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_RESOURCE_BLOBRANGE_HPP
