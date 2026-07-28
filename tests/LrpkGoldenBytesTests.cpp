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
  const U32 kHeadCrcValue = 0xEC1FF79BUL;
  const U32 kAxesCrcValue = 0xD4A21606UL;
  const U32 kIndexCrcValue = 0xD9EF13E9UL;
  const U32 kDataHeaderCrcValue = 0xD1B31A80UL;
  const U32 kBagCrcValue = 0x7B67A63EUL; // crc32 of the whole bag

  const std::size_t kGoldenTotal = 612;

  // Two assets in one bag, so `assetCount` and `bagCount` differ. With one of
  // each the two fields hold the same number in both HEAD and INDX, and a
  // writer and reader that had swapped them would produce and accept exactly
  // these bytes -- the mirrored field-order defect this test exists to catch
  // would have been the one thing it could not see. The second payload is not
  // a multiple of four, which also pins the intra-bag padding and a non-zero
  // row offset.
  const unsigned char kFirstPayload[4] = {'G', 'O', 'L', 'D'};
  const unsigned char kSecondPayload[6] = {'S', 'I', 'L', 'V', 'E', 'R'};
  const std::size_t kBagBytes = 12; // 4, padded to 4, then 6, padded to 8

  void PutU32(std::vector<unsigned char> &bytes, std::size_t at, U32 value)
  {
    WriteU32BE(&bytes[at], value);
  }

  void PutTag(std::vector<unsigned char> &bytes, std::size_t at, const char *tag)
  {
    std::memcpy(&bytes[at], tag, 4);
  }

  /** One bag, two assets, no axes. AXES is present because the version
      requires it even when empty. */
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
    PutU32(out, 16 + kHeadAssetCount, 2);
    PutU32(out, 16 + kHeadBagCount, 1);
    // 16 + 40 .. 511 stay zero: the head is a fixed 512 bytes whatever it holds.

    // AXES is required even empty, so its absence is corruption rather than a
    // degenerate default in which the most specialized row wins unconditionally.
    PutTag(out, 512, "AXES");
    PutU32(out, 516, 4);
    out[520] = 0; // axisCount
    // 521..523 reserved, zero.

    PutTag(out, 524, "INDX");
    PutU32(out, 528, 8 + 20 + 16 + 16);
    PutU32(out, 532, 1); // bagCount
    PutU32(out, 536, 2); // assetCount

    // BAGS[0]: 20 bytes. The offset is relative to the DATA payload.
    const std::size_t bagRow = 540;
    PutU32(out, bagRow + 0, 0);
    PutU32(out, bagRow + kBagStoredSize, static_cast<U32>(kBagBytes));
    PutU32(out, bagRow + kBagExpandedSize, static_cast<U32>(kBagBytes));
    PutU32(out, bagRow + kBagCrc, kBagCrcValue);
    out[bagRow + kBagCodec] = CODEC_NONE;
    // bagRow + 17 .. 19 stay zero.

    // Each ASST row is exactly 16 bytes, so indexing is a shift rather than a
    // multiply on 68k, and the offset is relative to the expanded bag.
    // Rows ascend by id, which the format specifies rather than leaving to the
    // implementation.
    const std::size_t firstRow = 560;
    PutU32(out, firstRow + kRowId, 1);
    PutU32(out, firstRow + kRowOffset, 0);
    PutU32(out, firstRow + kRowLength, sizeof(kFirstPayload));
    out[firstRow + kRowBag] = 0;
    out[firstRow + kRowKind] = static_cast<unsigned char>(ASSET_KIND_IMAGE);
    WriteU16BE(&out[firstRow + kRowAxes], 0); // writes no axis

    const std::size_t secondRow = 576;
    PutU32(out, secondRow + kRowId, 2);
    PutU32(out, secondRow + kRowOffset, 4); // after the first payload's padding
    PutU32(out, secondRow + kRowLength, sizeof(kSecondPayload));
    out[secondRow + kRowBag] = 0;
    out[secondRow + kRowKind] = static_cast<unsigned char>(ASSET_KIND_STRING);
    WriteU16BE(&out[secondRow + kRowAxes], 0);

    PutTag(out, 592, "DATA");
    PutU32(out, 596, static_cast<U32>(kBagBytes));
    std::memcpy(&out[600], kFirstPayload, sizeof(kFirstPayload));
    std::memcpy(&out[604], kSecondPayload, sizeof(kSecondPayload));
    // 610..611 are the second payload's padding to the alignment boundary.
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
  writer.addAsset(1, bag, ASSET_KIND_IMAGE, 0, kFirstPayload, sizeof(kFirstPayload));
  writer.addAsset(2, bag, ASSET_KIND_STRING, 0, kSecondPayload, sizeof(kSecondPayload));
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
  assert(reader.assetCount() == 2);
  assert(reader.idSpaceStamp() == kStamp);
  assert(reader.openBag(0) == Reader::BAG_OK);

  Facts facts;
  Asset asset;
  assert(reader.get(1, facts, asset) == Reader::GET_OK);
  assert(asset.kind == ASSET_KIND_IMAGE);
  assert(asset.length == sizeof(kFirstPayload));
  assert(std::memcmp(asset.bytes, kFirstPayload, sizeof(kFirstPayload)) == 0);

  // The second asset is what makes the row stride and the intra-bag padding
  // observable: it is reached through a non-zero offset the reader computes.
  assert(reader.get(2, facts, asset) == Reader::GET_OK);
  assert(asset.kind == ASSET_KIND_STRING);
  assert(asset.length == sizeof(kSecondPayload));
  assert(std::memcmp(asset.bytes, kSecondPayload, sizeof(kSecondPayload)) == 0);

  std::printf("testLrpkWireFormatMatchesAnIndependentlyAssembledPackage passed\n");
}
