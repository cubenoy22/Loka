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

        /** Bit 0 of the HEAD flags word. Clear means the package carries no
            CRCs and opens in unsafe mode: rot detection may be omitted,
            mistaken-identity detection may not (#185 §10). */
        const U32 kFlagHasCrc = 0x1UL;

        inline U32 FourCC(char a, char b, char c, char d)
        {
          return ((static_cast<U32>(static_cast<unsigned char>(a)) << 24) |
                  (static_cast<U32>(static_cast<unsigned char>(b)) << 16) |
                  (static_cast<U32>(static_cast<unsigned char>(c)) << 8) |
                  static_cast<U32>(static_cast<unsigned char>(d)));
        }

        /** A chunk whose 4CC starts with an uppercase letter is critical:
            refuse to open if it is unknown. Lowercase is ancillary and may be
            skipped, which is how the diagnostic name table costs nothing on a
            4 MB build (#185 §14). */
        inline bool IsCriticalChunk(U32 fourCC)
        {
          const unsigned char first = static_cast<unsigned char>((fourCC >> 24) & 0xFFUL);
          return first >= 'A' && first <= 'Z';
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
