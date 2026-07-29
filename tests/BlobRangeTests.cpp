#include "BlobRangeTests.hpp"

#include <cassert>
#include <cstdio>

#include "core/resource/BlobRange.hpp"

using loka::core::resource::BlobRangeIsUsable;

// Pinned here rather than through a platform context: the null platform
// decodes nothing and refuses every image request whatever the range, so
// whether a range was accepted is not observable through that seam. The four
// real decoders share this predicate so they cannot disagree about what a
// usable range is, which makes it the thing worth pinning.

void testBlobRangeRefusesWhatWouldReadOutsideTheBuffer()
{
  const std::size_t kMax = static_cast<std::size_t>(-1);

  // Ordinary ranges inside a 10-byte buffer.
  assert(BlobRangeIsUsable(10, 0, 10));
  assert(BlobRangeIsUsable(10, 0, 1));
  assert(BlobRangeIsUsable(10, 9, 1));
  assert(BlobRangeIsUsable(10, 4, 6));

  // One byte past the end, from either term.
  assert(!BlobRangeIsUsable(10, 0, 11));
  assert(!BlobRangeIsUsable(10, 4, 7));
  assert(!BlobRangeIsUsable(10, 11, 1));

  // An empty range is refused rather than admitted as an empty image. There is
  // no zero-byte asset to decode, and accepting one only moves the failure to
  // whichever decoder indexes the first byte.
  assert(!BlobRangeIsUsable(10, 0, 0));
  assert(!BlobRangeIsUsable(10, 10, 0));
  assert(!BlobRangeIsUsable(0, 0, 0));

  // The reason the predicate subtracts instead of adding. `offset + length`
  // wraps here to a small number, so a check written that way would accept a
  // range asking for the entire address space and hand it to a decoder.
  assert(!BlobRangeIsUsable(10, 4, kMax));
  assert(!BlobRangeIsUsable(10, kMax, 4));
  assert(!BlobRangeIsUsable(10, kMax, kMax));
  // A wrapping pair whose sum is exactly a legal-looking value.
  assert(!BlobRangeIsUsable(10, kMax - 3, 8));

  // `offset == size` is a legal cursor but never a legal range, because any
  // length from there is already past the end.
  assert(!BlobRangeIsUsable(10, 10, 1));

  std::printf("testBlobRangeRefusesWhatWouldReadOutsideTheBuffer passed\n");
}
