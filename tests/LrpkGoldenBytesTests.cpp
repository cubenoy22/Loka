#include "LrpkGoldenBytesTests.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "core/resource/lrpk/LrpkReader.hpp"
#include "lrpc/LrpkWriter.hpp"

using namespace loka::core::resource::lrpk;
using loka::lrpc::Writer;

// Every other LRPK test builds its package with the writer and reads it back
// with the reader, which cannot tell the two apart if they share one misreading
// of the format: a swapped field order, a native-endian store, a CRC covering
// the wrong extent, all round-trip perfectly and are all still the wrong file.
// The property at risk is the one the format exists for -- a package written on
// an x86 host is read byte for byte by a 68k target (#185 §14) -- so the round
// trip cannot be the only evidence.
//
// This package is assembled here from the documented layout instead. The four
// CRC values below were computed with zlib's crc32, an implementation
// independent of `loka::core::resource::lrpk::Crc32`, so the writer agreeing
// with them is a real cross-check rather than the class agreeing with itself.
// Reproduce with:
//
//   python3 -c "import zlib;print(hex(zlib.crc32(open('p','rb').read()[512:524])))"
//
// A package byte-identical to the one built below has been diffed against the
// writer's output for the same input; keeping the assembly explicit is what
// makes a future layout change reviewable rather than a re-recorded blob.

namespace
{
  const U32 kStamp = 0x11223344UL;

  // Computed by zlib, not by the class under test.
  const U32 kHeadCrcValue = 0xCB3DC86CUL;
  const U32 kAxesCrcValue = 0xD4A21606UL;
  const U32 kIndexCrcValue = 0x5ACF1003UL;
  const U32 kDataHeaderCrcValue = 0xDF6892B2UL;
  const U32 kBagCrcValue = 0x713CF0E5UL; // crc32("GOLD")

  const std::size_t kGoldenTotal = 588;
  const unsigned char kPayload[4] = {'G', 'O', 'L', 'D'};

  void PutU32(std::vector<unsigned char> &bytes, std::size_t at, U32 value)
  {
    WriteU32BE(&bytes[at], value);
  }

  void PutTag(std::vector<unsigned char> &bytes, std::size_t at, const char *tag)
  {
    std::memcpy(&bytes[at], tag, 4);
  }

  /** One bag, one asset, no axes -- the smallest package the format admits,
      because AXES is required by the version even at zero axes. */
  void AssembleGolden(std::vector<unsigned char> &out)
  {
    out.assign(kGoldenTotal, 0);

    // Form header: the 4CC and the length of everything after it.
    PutTag(out, 0, "LRPK");
    PutU32(out, 4, static_cast<U32>(kGoldenTotal - 8));

    // The fixed 512-byte head, so one sector read reaches every check value.
    PutTag(out, 8, "HEAD");
    PutU32(out, 12, static_cast<U32>(kHeadPayloadBytes));
    PutU32(out, 16 + kHeadCrc, kHeadCrcValue);
    PutU32(out, 16 + kHeadAxesCrc, kAxesCrcValue);
    PutU32(out, 16 + kHeadIndexCrc, kIndexCrcValue);
    PutU32(out, 16 + kHeadDataHeaderCrc, kDataHeaderCrcValue);
    PutU32(out, 16 + kHeadVersion, kFormatVersion);
    PutU32(out, 16 + kHeadTotalBytes, static_cast<U32>(kGoldenTotal));
    PutU32(out, 16 + kHeadIdSpaceStamp, kStamp);
    PutU32(out, 16 + kHeadFlags, 0);
    PutU32(out, 16 + kHeadAssetCount, 1);
    PutU32(out, 16 + kHeadBagCount, 1);
    // 16 + 40 .. 511 stay zero: the head is a fixed 512 bytes whatever it holds.

    // AXES is required even empty, so its absence is corruption rather than a
    // degenerate default in which the most specialized row wins unconditionally.
    PutTag(out, 512, "AXES");
    PutU32(out, 516, 4);
    out[520] = 0; // axisCount
    // 521..523 reserved, zero.

    PutTag(out, 524, "INDX");
    PutU32(out, 528, 8 + 20 + 16);
    PutU32(out, 532, 1); // bagCount
    PutU32(out, 536, 1); // assetCount

    // BAGS[0]: 20 bytes. The offset is relative to the DATA payload.
    const std::size_t bagRow = 540;
    PutU32(out, bagRow + 0, 0);
    PutU32(out, bagRow + kBagStoredSize, 4);
    PutU32(out, bagRow + kBagExpandedSize, 4);
    PutU32(out, bagRow + kBagCrc, kBagCrcValue);
    out[bagRow + kBagCodec] = CODEC_NONE;
    // bagRow + 17 .. 19 stay zero.

    // ASST[0]: exactly 16 bytes, so indexing is a shift rather than a multiply
    // on 68k. The offset is relative to the expanded bag.
    const std::size_t assetRow = 560;
    PutU32(out, assetRow + kRowId, 1);
    PutU32(out, assetRow + kRowOffset, 0);
    PutU32(out, assetRow + kRowLength, 4);
    out[assetRow + kRowBag] = 0;
    out[assetRow + kRowKind] = static_cast<unsigned char>(ASSET_KIND_IMAGE);
    WriteU16BE(&out[assetRow + kRowAxes], 0); // writes no axis

    PutTag(out, 576, "DATA");
    PutU32(out, 580, 4);
    std::memcpy(&out[584], kPayload, sizeof(kPayload));
  }

  /** `openBorrowedBytes` refuses a base that is not `kPayloadAlign`-aligned,
      and a vector's data has no such guarantee. */
  const unsigned char *AlignedCopy(const std::vector<unsigned char> &source,
                                   std::vector<unsigned char> &backing)
  {
    backing.assign(source.size() + kPayloadAlign, 0);
    std::size_t at = 0;
    while ((reinterpret_cast<std::size_t>(&backing[at]) % kPayloadAlign) != 0)
    {
      ++at;
    }
    std::memcpy(&backing[at], &source[0], source.size());
    return &backing[at];
  }
} // namespace

void testLrpkWireFormatMatchesAnIndependentlyAssembledPackage()
{
  std::vector<unsigned char> golden;
  AssembleGolden(golden);
  assert(golden.size() == kGoldenTotal);

  // The writer must produce this file, not merely a file its own reader likes.
  Writer writer;
  const std::size_t bag = writer.addBag();
  writer.addAsset(1, bag, ASSET_KIND_IMAGE, 0, kPayload, sizeof(kPayload));
  std::vector<unsigned char> built;
  assert(writer.build(kStamp, built) == Writer::BUILD_OK);
  assert(built.size() == golden.size());
  assert(std::memcmp(&built[0], &golden[0], golden.size()) == 0);

  // And the reader must accept the assembled bytes, so the pin holds both
  // halves against the same external description rather than against each
  // other. Integrity is verified, which is what makes the four independently
  // computed CRC values load-bearing here.
  std::vector<unsigned char> backing;
  const unsigned char *bytes = AlignedCopy(golden, backing);

  Reader reader;
  assert(reader.openBorrowedBytes(bytes, golden.size(), kStamp, Reader::VERIFY_INTEGRITY) ==
         Reader::OPEN_OK);
  assert(reader.bagCount() == 1);
  assert(reader.assetCount() == 1);
  assert(reader.idSpaceStamp() == kStamp);
  assert(reader.openBag(0) == Reader::BAG_OK);

  Facts facts;
  Asset asset;
  assert(reader.get(1, facts, asset) == Reader::GET_OK);
  assert(asset.kind == ASSET_KIND_IMAGE);
  assert(asset.length == sizeof(kPayload));
  assert(std::memcmp(asset.bytes, kPayload, sizeof(kPayload)) == 0);

  std::printf("testLrpkWireFormatMatchesAnIndependentlyAssembledPackage passed\n");
}
