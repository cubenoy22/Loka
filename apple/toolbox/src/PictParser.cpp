#include "PictParser.hpp"

namespace
{
  static unsigned short ReadU16BE(const unsigned char *p)
  {
    return static_cast<unsigned short>(
        (static_cast<unsigned short>(p[0]) << 8)
        | static_cast<unsigned short>(p[1]));
  }

  static short ReadS16BE(const unsigned char *p)
  {
    return static_cast<short>(ReadU16BE(p));
  }

  static std::size_t FindPictSizeByTerminator(
      const std::vector<unsigned char> &bytes,
      std::size_t offset,
      std::size_t limit)
  {
    // PICT end opcode is 0x00FF on word boundary. The scan stops at the range's
    // end rather than the buffer's, so a picture inside a bag cannot be given
    // an extent that runs into the asset stored after it.
    if (offset + 12 > limit)
    {
      return 0;
    }
    std::size_t last = 0;
    for (std::size_t pos = offset + 10; pos + 1 < limit; pos += 2)
    {
      if (bytes[pos] == 0x00 && bytes[pos + 1] == 0xFF)
      {
        last = (pos + 2) - offset;
      }
    }
    return last;
  }

  static bool HasPictVersionOpcode(
      const std::vector<unsigned char> &bytes,
      std::size_t offset,
      std::size_t limit)
  {
    const std::size_t length = limit - offset;
    if (length >= 14
        && bytes[offset + 10] == 0x00
        && bytes[offset + 11] == 0x11
        && bytes[offset + 12] == 0x02
        && bytes[offset + 13] == 0xFF)
    {
      return true;
    }
    return length >= 12
        && bytes[offset + 10] == 0x11
        && bytes[offset + 11] == 0x01;
  }

  static bool TryParsePictAt(
      const std::vector<unsigned char> &bytes,
      std::size_t limit,
      std::size_t offset,
      loka::toolbox::pict::PictParseResult &out,
      bool &hasVersionOpcodeOut)
  {
    if (offset + 10 > limit)
    {
      return false;
    }

    const unsigned char *base = &bytes[offset];
    const unsigned short pictSize = ReadU16BE(base);
    std::size_t pictureSize = static_cast<std::size_t>(pictSize);

    const short top = ReadS16BE(base + 2);
    const short left = ReadS16BE(base + 4);
    const short bottom = ReadS16BE(base + 6);
    const short right = ReadS16BE(base + 8);

    int width = static_cast<int>(right) - static_cast<int>(left);
    int height = static_cast<int>(bottom) - static_cast<int>(top);
    if (width < 0)
    {
      width = -width;
    }
    if (height < 0)
    {
      height = -height;
    }
    if (width == 0 || height == 0)
    {
      return false;
    }

    if (!(pictureSize >= 10 && pictureSize <= limit - offset))
    {
      pictureSize = FindPictSizeByTerminator(bytes, offset, limit);
      if (!(pictureSize >= 10 && pictureSize <= limit - offset))
      {
        // Keep stream path permissive: if size field/terminator are unreliable,
        // draw from the rest of the RANGE like SimpleText's file-backed path.
        // Bounded by the range and not the buffer, so a permissive extent still
        // cannot cross into the next asset in a bag.
        pictureSize = limit - offset;
        if (pictureSize < 10)
        {
          return false;
        }
      }
    }

    out.pictureOffset = offset;
    out.pictureSize = pictureSize;
    out.width = width;
    out.height = height;
    hasVersionOpcodeOut = HasPictVersionOpcode(bytes, offset, limit);
    return true;
  }
}

namespace loka
{
  namespace toolbox
  {
    namespace pict
    {
      PictParseResult::PictParseResult()
          : pictureOffset(0),
            pictureSize(0),
            width(0),
            height(0)
      {
      }

      bool ParsePict(const std::vector<unsigned char> &bytes,
                     std::size_t base,
                     std::size_t limit,
                     PictParseResult &out)
      {
        if (base > limit || limit > bytes.size())
        {
          return false;
        }

        // Offsets stay absolute within the blob -- `base` is where the range
        // starts, not a new origin. One coordinate system end to end is what
        // keeps the payload from having to add a base back on and double-count
        // it.
        PictParseResult rawCandidate;
        bool rawHasVersionOpcode = false;
        const bool hasRawCandidate = TryParsePictAt(
            bytes, limit, base, rawCandidate, rawHasVersionOpcode);

        PictParseResult headeredCandidate;
        bool headeredHasVersionOpcode = false;
        const bool hasHeaderedCandidate = limit - base > 522
            && TryParsePictAt(
                bytes,
                limit,
                base + 512,
                headeredCandidate,
                headeredHasVersionOpcode);

        // A plausible frame is not enough to distinguish a PICT from the
        // arbitrary application bytes in a 512-byte file header. A versioned
        // headered stream wins the corroborated tie; otherwise keep a
        // versioned raw stream at the range base.
        if (hasHeaderedCandidate && headeredHasVersionOpcode)
        {
          out = headeredCandidate;
          return true;
        }
        if (hasRawCandidate && rawHasVersionOpcode)
        {
          out = rawCandidate;
          return true;
        }
        return false;
      }
    }
  }
}
