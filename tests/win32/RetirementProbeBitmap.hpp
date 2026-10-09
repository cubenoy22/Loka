#ifndef LOKA_TESTS_WIN32_RETIREMENT_PROBE_BITMAP_HPP
#define LOKA_TESTS_WIN32_RETIREMENT_PROBE_BITMAP_HPP

#include <cstring>
#include "core/resource/Blob.hpp"

namespace retirement_probe
{
  inline void Put32(unsigned char *bytes, unsigned long value)
  {
    for (int i = 0; i != 4; ++i) bytes[i] = static_cast<unsigned char>(value >> (8 * i));
  }

  /** Original uncompressed 32bpp BMP fixture. Callers use fixed, small sizes. */
  inline loka::core::resource::Blob Bitmap(int width, int height, unsigned char fill)
  {
    using loka::core::resource::Blob;
    const unsigned long pixelBytes = static_cast<unsigned long>(width) * height * 4UL;
    Blob blob = Blob::Create();
    if (!blob.tryResize(54 + pixelBytes)) return Blob();
    unsigned char *bytes = blob.mutableData();
    std::memset(bytes, 0, 54 + pixelBytes);
    bytes[0] = 'B'; bytes[1] = 'M';
    Put32(bytes + 2, 54 + pixelBytes);
    Put32(bytes + 10, 54);
    Put32(bytes + 14, 40);
    Put32(bytes + 18, static_cast<unsigned long>(width));
    Put32(bytes + 22, static_cast<unsigned long>(height));
    bytes[26] = 1; bytes[28] = 32;
    Put32(bytes + 34, pixelBytes);
    std::memset(bytes + 54, fill, pixelBytes);
    return blob;
  }
}
#endif
