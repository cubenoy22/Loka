#ifndef LOKA_CORE_RESOURCE_LRPK_LRPKFORMAT_HPP
#define LOKA_CORE_RESOURCE_LRPK_LRPKFORMAT_HPP

#include <cstddef>

namespace loka
{
  namespace core
  {
    namespace resource
    {
      namespace lrpk
      {
        /** 32-bit field type for the on-disk format. `unsigned long` rather
            than a fixed-width typedef because the Classic toolchains predate
            <stdint.h>; every value is masked to 32 bits on the way in and out
            so a 64-bit `unsigned long` behaves identically. */
        typedef unsigned long U32;

        const U32 kU32Mask = 0xFFFFFFFFUL;

        /** Format version of the container itself. Bumped only when a reader
            that does not know the new version must refuse to open. */
        const U32 kFormatVersion = 1;

        /** Fixed head: the form header plus the HEAD chunk, padded to this
            size so one 512-byte sector read yields every check value (#185
            §10). This covers the header only — the index is read and CRC'd
            separately and may be much larger. */
        const std::size_t kFixedHeadBytes = 512;

        /** Payload alignment. IFF pads to even boundaries; the 68000 faults on
            odd-address word access and unaligned payloads slow drawing, so
            align to 4 (#185 §14). */
        const std::size_t kPayloadAlign = 4;

        /** One asset row is exactly this wide so indexing is a shift rather
            than a multiply on 68k: id(4) offset(4) len(4) bag(1) kind(1)
            axes(2). */
        const std::size_t kAssetRowBytes = 16;

        /** One bag row: dataOffset(4) storedSize(4) expandedSize(4) crc(4)
            codec(1) flags(1) reserved(2). */
        const std::size_t kBagRowBytes = 20;

        /** Axes are encoded as one nibble per declared axis inside the row's
            16-bit `axes` field, with 0 meaning "this row does not write that
            axis". Four axes is the ceiling, which matches the expected rep
            count of one normally and at most about four (#185 §14). */
        const std::size_t kMaxAxes = 4;
        const std::size_t kMaxAxisValues = 15;
        const std::size_t kMaxBags = 16;

        /** V1 layout facts shared by the writer, reader, and byte-level pins.
            Keeping these in one place prevents the two sides of the format
            from silently assigning different meanings to the same byte. */
        const std::size_t kFormHeaderBytes = 8;
        const std::size_t kChunkHeaderBytes = 8;
        const std::size_t kHeadPayloadBytes = kFixedHeadBytes - kFormHeaderBytes - kChunkHeaderBytes;
        const std::size_t kHeadPayloadOffset = kFormHeaderBytes + kChunkHeaderBytes;

        // HEAD payload. headCrc is first and covers the HEAD chunk header plus
        // every payload byte after the check value.
        const std::size_t kHeadCrc = 0;
        const std::size_t kHeadAxesCrc = 4;
        const std::size_t kHeadIndexCrc = 8;
        const std::size_t kHeadDataHeaderCrc = 12;
        const std::size_t kHeadVersion = 16;
        const std::size_t kHeadTotalBytes = 20;
        const std::size_t kHeadIdSpaceStamp = 24;
        const std::size_t kHeadFlags = 28;
        const std::size_t kHeadAssetCount = 32;
        const std::size_t kHeadBagCount = 36;

        // One fixed-width AXES entry. Slot is its physical position; rank is
        // the independent package-owned precedence policy.
        const std::size_t kAxisEntryBytes = 40;
        const std::size_t kAxisKind = 0;
        const std::size_t kAxisValueCount = 1;
        const std::size_t kAxisPrecedenceRank = 2;
        const std::size_t kAxisBaseline = 4;
        const std::size_t kAxisValues = 8;

        // INDX bag row.
        const std::size_t kBagDataOffset = 0;
        const std::size_t kBagStoredSize = 4;
        const std::size_t kBagExpandedSize = 8;
        const std::size_t kBagCrc = 12;
        const std::size_t kBagCodec = 16;

        // INDX representation row.
        const std::size_t kRowId = 0;
        const std::size_t kRowOffset = 4;
        const std::size_t kRowLength = 8;
        const std::size_t kRowBag = 12;
        const std::size_t kRowKind = 13;
        const std::size_t kRowAxes = 14;

        /** Axis kinds. The selection rule has exactly one rule per kind, which
            is why the kind travels in the package rather than being inferred
            (#185 §6, §14). */
        enum AxisKind
        {
          AXIS_KIND_ENUM = 0,
          AXIS_KIND_SCALAR = 1
        };

        /** Asset kinds carried in the row. Redundant with the bytes themselves;
            it keeps scanning cheap and `lrpc` guarantees it agrees across all
            rows of one id. */
        enum AssetKind
        {
          ASSET_KIND_UNKNOWN = 0,
          ASSET_KIND_IMAGE = 1,
          ASSET_KIND_STRING = 2,
          ASSET_KIND_AUDIO = 3
        };

        /** V1 emits `none`; the field is reserved so adding a codec later
            leaves call sites unchanged (#185 §11). */
        enum Codec
        {
          CODEC_NONE = 0,
          CODEC_RLE = 1
        };

        inline U32 FourCC(char a, char b, char c, char d)
        {
          return ((static_cast<U32>(static_cast<unsigned char>(a)) << 24) |
                  (static_cast<U32>(static_cast<unsigned char>(b)) << 16) |
                  (static_cast<U32>(static_cast<unsigned char>(c)) << 8) |
                  static_cast<U32>(static_cast<unsigned char>(d)));
        }

        /** Big-endian readers. Written bytewise rather than by casting to a
            wider type so no alignment is assumed, matching how PICT parsing
            already reads words in this repository. Big-endian means 68k does
            no swapping at all; the cost falls on x86/ARM, which has the
            headroom (#185 §14). */
        inline U32 ReadU16BE(const unsigned char *p)
        {
          return ((static_cast<U32>(p[0]) << 8) | static_cast<U32>(p[1]));
        }

        inline U32 ReadU32BE(const unsigned char *p)
        {
          return (((static_cast<U32>(p[0]) << 24) | (static_cast<U32>(p[1]) << 16) |
                   (static_cast<U32>(p[2]) << 8) | static_cast<U32>(p[3])) &
                  kU32Mask);
        }

        inline void WriteU16BE(unsigned char *p, U32 value)
        {
          p[0] = static_cast<unsigned char>((value >> 8) & 0xFFUL);
          p[1] = static_cast<unsigned char>(value & 0xFFUL);
        }

        inline void WriteU32BE(unsigned char *p, U32 value)
        {
          p[0] = static_cast<unsigned char>((value >> 24) & 0xFFUL);
          p[1] = static_cast<unsigned char>((value >> 16) & 0xFFUL);
          p[2] = static_cast<unsigned char>((value >> 8) & 0xFFUL);
          p[3] = static_cast<unsigned char>(value & 0xFFUL);
        }

        /** Overflow-safe extent checks. Every bound in the reader goes through
            these rather than computing `offset + length` or `count * width`
            first: on a 32-bit target those arithmetic results wrap, and a
            forged index can pick values whose wrapped result passes a naive
            comparison. Having one place for the rule is the point -- the
            alternative is applying it by hand to whichever site someone
            happened to name. */
        inline bool ExtentFits(std::size_t total, std::size_t offset, std::size_t length)
        {
          return offset <= total && length <= total - offset;
        }

        inline bool ProductFits(std::size_t total, std::size_t count, std::size_t width)
        {
          if (width == 0)
          {
            return true;
          }
          return count <= total / width;
        }

        /** Tests a value expressed as two 32-bit words without depending on
            the host width. This is the directly pinnable rule behind
            SizeFitsU32: a non-zero high word cannot be serialized in a V1
            length field. */
        inline bool U32WordsFit(U32 highWord, U32 lowWord)
        {
          return highWord == 0 && lowWord <= kU32Mask;
        }

        /** True when a value held in the Classic-compatible U32 host type can
            be serialized without narrowing. `unsigned long` is wider than the
            format field on LP64 pack hosts. */
        inline bool U32ValueFits(U32 value)
        {
          U32 remaining = value;
          for (std::size_t byte = 0; byte < 4; ++byte)
          {
            remaining >>= 8;
          }
          return remaining == 0;
        }

        /** True when a host size can be represented without narrowing in a
            32-bit on-disk field. Bytewise splitting avoids assuming that the
            host's size_t is itself wider than 16 bits. */
        inline bool SizeFitsU32(std::size_t value)
        {
          std::size_t remaining = value;
          U32 lowWord = 0;
          for (std::size_t byte = 0; byte < 4; ++byte)
          {
            lowWord |= static_cast<U32>(remaining & static_cast<std::size_t>(0xFFUL)) << (byte * 8);
            remaining >>= 8;
          }
          const U32 highWord = static_cast<U32>(remaining);
          return U32WordsFit(highWord, lowWord);
        }

        inline std::size_t AlignUp(std::size_t value, std::size_t alignment)
        {
          const std::size_t remainder = value % alignment;
          return remainder == 0 ? value : value + (alignment - remainder);
        }

        /** CRC-32, the zlib/PNG polynomial, nibble table: 64 bytes of table
            rather than 1 KB, which matters on 68k. Not a cryptographic hash on
            purpose — the goal is detecting fast that something which should be
            there is gone, and calling it a hash would invite the wrong
            expectation (#185 §10). */
        class Crc32
        {
        public:
          Crc32()
              : value_(0xFFFFFFFFUL)
          {
          }

          void update(const unsigned char *bytes, std::size_t length)
          {
            static const U32 kNibbleTable[16] = {
                0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL, 0x76DC4190UL, 0x6B6B51F4UL,
                0x4DB26158UL, 0x5005713CUL, 0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
                0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL};
            if (!bytes)
            {
              return;
            }
            U32 crc = value_;
            for (std::size_t i = 0; i < length; ++i)
            {
              crc = crc ^ static_cast<U32>(bytes[i]);
              crc = ((crc >> 4) & 0x0FFFFFFFUL) ^ kNibbleTable[crc & 0x0FUL];
              crc = ((crc >> 4) & 0x0FFFFFFFUL) ^ kNibbleTable[crc & 0x0FUL];
            }
            value_ = crc;
          }

          U32 value() const
          {
            return (value_ ^ 0xFFFFFFFFUL) & kU32Mask;
          }

          static U32 Of(const unsigned char *bytes, std::size_t length)
          {
            Crc32 crc;
            crc.update(bytes, length);
            return crc.value();
          }

        private:
          U32 value_;
        };
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_RESOURCE_LRPK_LRPKFORMAT_HPP
