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
  const U32 kHeadCrcValue = 0x679E08C6UL;
  const U32 kAxesCrcValue = 0xD4A21606UL;
  const U32 kIndexCrcValue = 0x2D237C0AUL;
  const U32 kDataHeaderCrcValue = 0xC2DF82D6UL;
  const U32 kFirstBagCrcValue = 0x7B67A63EUL;
  const U32 kSecondBagCrcValue = 0x71FC1C1BUL;

  const std::size_t kGoldenTotal = 656;

  // Three assets across two bags, so the three scalars a mirrored field swap
  // could hide behind -- `version`, `assetCount`, `bagCount` -- hold 1, 3 and 2
  // rather than sharing a value. An earlier shape used one bag and one asset,
  // where all three read 1 and a writer and reader that had swapped any pair
  // produced and accepted the asserted bytes unchanged. A fixture cannot make
  // every field distinct (the zero-valued ones never will be), but the ones a
  // swap would silently destroy must be.
  //
  // The payload lengths are deliberately not multiples of four, which makes the
  // intra-bag padding, a non-zero row offset and a non-zero bag offset all
  // observable.
  const unsigned char kGold[4] = {'G', 'O', 'L', 'D'};
  const unsigned char kSilver[6] = {'S', 'I', 'L', 'V', 'E', 'R'};
  const unsigned char kBronze[6] = {'B', 'R', 'O', 'N', 'Z', 'E'};
  const std::size_t kFirstBagBytes = 12;  // 4 padded to 4, then 6 padded to 8
  const std::size_t kSecondBagBytes = 8;  // 6 padded to 8

  void PutU32(std::vector<unsigned char> &bytes, std::size_t at, U32 value)
  {
    WriteU32BE(&bytes[at], value);
  }

  void PutTag(std::vector<unsigned char> &bytes, std::size_t at, const char *tag)
  {
    std::memcpy(&bytes[at], tag, 4);
  }

  void PutRow(std::vector<unsigned char> &out,
              std::size_t at,
              U32 id,
              U32 offset,
              U32 length,
              unsigned char bag,
              AssetKind kind)
  {
    PutU32(out, at + kRowId, id);
    PutU32(out, at + kRowOffset, offset);
    PutU32(out, at + kRowLength, length);
    out[at + kRowBag] = bag;
    out[at + kRowKind] = static_cast<unsigned char>(kind);
    WriteU16BE(&out[at + kRowAxes], 0); // writes no axis
  }

  void PutBagRow(std::vector<unsigned char> &out, std::size_t at, U32 dataOffset, U32 size, U32 crc)
  {
    PutU32(out, at + kBagDataOffset, dataOffset);
    PutU32(out, at + kBagStoredSize, size);
    PutU32(out, at + kBagExpandedSize, size);
    PutU32(out, at + kBagCrc, crc);
    out[at + kBagCodec] = CODEC_NONE;
    // at + 17 .. 19 stay zero.
  }

  /** Two bags, three assets, no axes. AXES is present because the version
      requires it even when empty. */
  void AssembleGolden(std::vector<unsigned char> &out)
  {
    out.assign(kGoldenTotal, 0);

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
    PutU32(out, 16 + kHeadAssetCount, 3);
    PutU32(out, 16 + kHeadBagCount, 2);
    // 16 + 40 .. 511 stay zero.

    // AXES is required even empty, so its absence is corruption rather than a
    // degenerate default in which the most specialized row wins unconditionally.
    PutTag(out, 512, "AXES");
    PutU32(out, 516, 4);
    out[520] = 0; // axisCount, then 3 reserved zero bytes

    PutTag(out, 524, "INDX");
    PutU32(out, 528, 8 + 20 * 2 + 16 * 3);
    PutU32(out, 532, 2); // bagCount
    PutU32(out, 536, 3); // assetCount

    // Bag offsets are relative to the DATA payload, so the second bag's is
    // non-zero and a base confusion cannot hide.
    PutBagRow(out, 540, 0, static_cast<U32>(kFirstBagBytes), kFirstBagCrcValue);
    PutBagRow(out, 560, static_cast<U32>(kFirstBagBytes),
              static_cast<U32>(kSecondBagBytes), kSecondBagCrcValue);

    // Rows ascend by id. Each is exactly 16 bytes, so indexing is a shift
    // rather than a multiply on 68k, and `offset` is relative to its own
    // expanded bag -- which is why the third row starts at zero again.
    PutRow(out, 580, 1, 0, sizeof(kGold), 0, ASSET_KIND_IMAGE);
    PutRow(out, 596, 2, 4, sizeof(kSilver), 0, ASSET_KIND_STRING);
    PutRow(out, 612, 3, 0, sizeof(kBronze), 1, ASSET_KIND_IMAGE);

    PutTag(out, 628, "DATA");
    PutU32(out, 632, static_cast<U32>(kFirstBagBytes + kSecondBagBytes));
    std::memcpy(&out[636], kGold, sizeof(kGold));
    std::memcpy(&out[640], kSilver, sizeof(kSilver));
    // 646..647 pad the first bag to its 12 bytes.
    std::memcpy(&out[648], kBronze, sizeof(kBronze));
    // 654..655 pad the second bag to its 8.
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
  const std::size_t first = writer.addBag();
  const std::size_t second = writer.addBag();
  writer.addAsset(1, first, ASSET_KIND_IMAGE, 0, kGold, sizeof(kGold));
  writer.addAsset(2, first, ASSET_KIND_STRING, 0, kSilver, sizeof(kSilver));
  writer.addAsset(3, second, ASSET_KIND_IMAGE, 0, kBronze, sizeof(kBronze));
  std::vector<unsigned char> built;
  assert(writer.build(kStamp, built) == Writer::BUILD_OK);
  assert(built.size() == golden.size());
  assert(std::memcmp(&built[0], &golden[0], golden.size()) == 0);

  // And the reader must accept the assembled bytes, so the pin holds both
  // halves against the same external description rather than against each
  // other. Integrity is verified, which is what makes the independently
  // computed CRC values load-bearing here.
  std::vector<unsigned char> backing;
  const unsigned char *bytes = AlignedCopy(golden, backing);

  Reader reader;
  assert(reader.openBorrowedBytes(bytes, golden.size(), kStamp, Reader::VERIFY_INTEGRITY) ==
         Reader::OPEN_OK);
  assert(reader.bagCount() == 2);
  assert(reader.assetCount() == 3);
  assert(reader.idSpaceStamp() == kStamp);
  assert(reader.openBag(0) == Reader::BAG_OK);
  assert(reader.openBag(1) == Reader::BAG_OK);

  Facts facts;
  Asset asset;
  assert(reader.get(1, facts, asset) == Reader::GET_OK);
  assert(asset.kind == ASSET_KIND_IMAGE && asset.length == sizeof(kGold));
  assert(std::memcmp(asset.bytes, kGold, sizeof(kGold)) == 0);

  // Reached through a non-zero row offset inside the first bag.
  assert(reader.get(2, facts, asset) == Reader::GET_OK);
  assert(asset.kind == ASSET_KIND_STRING && asset.length == sizeof(kSilver));
  assert(std::memcmp(asset.bytes, kSilver, sizeof(kSilver)) == 0);

  // And this one through a non-zero *bag* offset, so a reader that confused
  // the two bases would fail here rather than pass by coincidence.
  assert(reader.get(3, facts, asset) == Reader::GET_OK);
  assert(asset.kind == ASSET_KIND_IMAGE && asset.length == sizeof(kBronze));
  assert(std::memcmp(asset.bytes, kBronze, sizeof(kBronze)) == 0);

  std::printf("testLrpkWireFormatMatchesAnIndependentlyAssembledPackage passed\n");
}
