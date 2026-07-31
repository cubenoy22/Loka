#include "LrpkGoldenBytesTests.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "LrpkTestByteSource.hpp"
#include "core/resource/lrpk/LrpkReader.hpp"
#include "core/resource/lrpk/LrpkStdioByteSource.hpp"
#include "lrpc/LrpkWriter.hpp"

using namespace loka::core::resource::lrpk;
using loka::lrpc::AssetLayoutKey;
using loka::lrpc::Writer;
using loka::lrpktests::MemoryByteSource;

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

  /** The three assets the golden package carries, as the reader must report
      them whatever door they came through. */
  struct ExpectedAsset
  {
    U32 id;
    const unsigned char *bytes;
    std::size_t length;
    std::size_t bag;
    std::size_t offsetInBag;
    AssetKind kind;
  };

  const ExpectedAsset kExpected[3] = {
      {1, kGold, sizeof(kGold), 0, 0, ASSET_KIND_IMAGE},
      {2, kSilver, sizeof(kSilver), 0, 4, ASSET_KIND_STRING},
      {3, kBronze, sizeof(kBronze), 1, 0, ASSET_KIND_IMAGE},
  };

  /** Everything a caller can observe about an opened golden package, checked
      the same way for every transport so a divergence is one failed assert
      rather than three near-identical bodies that drifted apart. */
  void ExpectGoldenContents(Reader &reader)
  {
    assert(reader.isOpen());
    assert(reader.bagCount() == 2);
    assert(reader.assetCount() == 3);
    assert(reader.idSpaceStamp() == kStamp);

    Facts facts;
    for (std::size_t i = 0; i < 3; ++i)
    {
      Asset asset;
      assert(reader.get(kExpected[i].id, facts, asset) == Reader::GET_OK);
      assert(asset.kind == kExpected[i].kind);
      assert(asset.length == kExpected[i].length);
      assert(asset.bag == kExpected[i].bag);
      assert(asset.offsetInBag == kExpected[i].offsetInBag);
      assert(std::memcmp(asset.bytes,
                         kExpected[i].bytes,
                         kExpected[i].length) == 0);
    }
  }

  /** Opens the golden bytes out of `source` and loads both bags into the
      caller's vectors, which must outlive `reader`. */
  void OpenGoldenFromSource(Reader &reader,
                            ByteSource &source,
                            std::vector<unsigned char> &index,
                            std::vector<unsigned char> &firstBag,
                            std::vector<unsigned char> &secondBag)
  {
    std::size_t need = 0;
    assert(reader.beginOpen(source, kStamp, Reader::VERIFY_INTEGRITY, need) ==
           Reader::OPEN_OK);
    // 512 + 8 + alignUp(4) + 8 + alignUp(56) + 8 = 636, the DATA payload
    // start. The spec's worked example, pinned as a number rather than as a
    // recomputation of the same formula.
    assert(need == 124);
    index.assign(need, 0);
    assert(reader.finishOpen(&index[0], need) == Reader::OPEN_OK);

    std::size_t stored = 0;
    assert(reader.bagStoredSize(0, stored) && stored == kFirstBagBytes);
    firstBag.assign(stored, 0);
    assert(reinterpret_cast<std::size_t>(&firstBag[0]) % kPayloadAlign == 0);
    assert(reader.readBagInto(0, &firstBag[0], stored) == Reader::BAG_OK);

    assert(reader.bagStoredSize(1, stored) && stored == kSecondBagBytes);
    secondBag.assign(stored, 0);
    assert(reinterpret_cast<std::size_t>(&secondBag[0]) % kPayloadAlign == 0);
    assert(reader.readBagInto(1, &secondBag[0], stored) == Reader::BAG_OK);
  }

  bool WriteWholeFile(const char *path, const std::vector<unsigned char> &bytes)
  {
    std::FILE *file = std::fopen(path, "wb");
    if (!file)
    {
      return false;
    }
    const std::size_t written =
        bytes.empty() ? 0 : std::fwrite(&bytes[0], 1, bytes.size(), file);
    return std::fclose(file) == 0 && written == bytes.size();
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
  writer.addAsset(AssetLayoutKey(""), 1, first, ASSET_KIND_IMAGE, 0, kGold, sizeof(kGold));
  writer.addAsset(AssetLayoutKey(""), 2, first, ASSET_KIND_STRING, 0, kSilver, sizeof(kSilver));
  writer.addAsset(AssetLayoutKey(""), 3, second, ASSET_KIND_IMAGE, 0, kBronze, sizeof(kBronze));
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

void testLrpkDataLayoutFollowsSourcePathInsteadOfId()
{
  const unsigned char aPayload = 'A';
  const unsigned char zPayload = 'Z';

  Writer writer;
  const std::size_t bag = writer.addBag();
  writer.addAsset(AssetLayoutKey("Assets/A.bin"),
                  2, bag, ASSET_KIND_IMAGE, 0, &aPayload, 1);
  writer.addAsset(AssetLayoutKey("Assets/Z.bin"),
                  1, bag, ASSET_KIND_IMAGE, 0, &zPayload, 1);

  std::vector<unsigned char> package;
  assert(writer.build(kStamp, package) == Writer::BUILD_OK);
  std::vector<unsigned char> backing;
  const unsigned char *bytes = AlignedCopy(package, backing);

  Reader reader;
  assert(reader.openBorrowedBytes(bytes,
                                  package.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  assert(reader.openBag(bag) == Reader::BAG_OK);

  Facts facts;
  Asset asset;
  assert(reader.get(2, facts, asset) == Reader::GET_OK);
  assert(asset.offsetInBag == 0);
  assert(reader.get(1, facts, asset) == Reader::GET_OK);
  assert(asset.offsetInBag == kPayloadAlign);

  // An explicit declaration order is stronger than the path fallback.
  Writer orderedWriter;
  const std::size_t orderedBag = orderedWriter.addBag();
  orderedWriter.addAsset(AssetLayoutKey(1, "Assets/A.bin"),
                         2, orderedBag, ASSET_KIND_IMAGE, 0, &aPayload, 1);
  orderedWriter.addAsset(AssetLayoutKey(0, "Assets/Z.bin"),
                         1, orderedBag, ASSET_KIND_IMAGE, 0, &zPayload, 1);
  std::vector<unsigned char> orderedPackage;
  assert(orderedWriter.build(kStamp, orderedPackage) == Writer::BUILD_OK);
  std::vector<unsigned char> orderedBacking;
  const unsigned char *orderedBytes = AlignedCopy(orderedPackage, orderedBacking);
  Reader orderedReader;
  assert(orderedReader.openBorrowedBytes(orderedBytes,
                                         orderedPackage.size(),
                                         kStamp,
                                         Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  assert(orderedReader.openBag(orderedBag) == Reader::BAG_OK);
  assert(orderedReader.get(1, facts, asset) == Reader::GET_OK);
  assert(asset.offsetInBag == 0);
  assert(orderedReader.get(2, facts, asset) == Reader::GET_OK);
  assert(asset.offsetInBag == kPayloadAlign);

  std::printf("testLrpkDataLayoutFollowsSourcePathInsteadOfId passed\n");
}

// The round-trip pin above proves the reader and the writer agree about these
// bytes through one door. This one proves the reader agrees with itself
// through all three -- a borrowed buffer, an arbitrary `ByteSource`, and a
// real file on the host's filesystem. The fixture is the same 656 bytes, so a
// difference in the answers can only come from the transport, which is the
// only thing that is allowed to differ.
void testLrpkGoldenBytesReadTheSameThroughEveryTransport()
{
  std::vector<unsigned char> golden;
  AssembleGolden(golden);

  std::vector<unsigned char> backing;
  const unsigned char *bytes = AlignedCopy(golden, backing);
  Reader memory;
  assert(memory.openBorrowedBytes(bytes,
                                  golden.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  assert(memory.openBag(0) == Reader::BAG_OK);
  assert(memory.openBag(1) == Reader::BAG_OK);
  ExpectGoldenContents(memory);

  // The source and every buffer are declared ahead of the reader that
  // borrows them, which is the lifetime contract stated as code.
  MemoryByteSource source(golden);
  std::vector<unsigned char> index;
  std::vector<unsigned char> firstBag;
  std::vector<unsigned char> secondBag;
  Reader stream;
  OpenGoldenFromSource(stream, source, index, firstBag, secondBag);
  ExpectGoldenContents(stream);

  // The bag buffers hold the file's bag bytes, not a re-derivation of them.
  assert(std::memcmp(&firstBag[0], &golden[636], kFirstBagBytes) == 0);
  assert(std::memcmp(&secondBag[0], &golden[648], kSecondBagBytes) == 0);

  // And the same, over stdio, against bytes that made a round trip through
  // the filesystem -- the transport the shipped reader will actually use.
  // Each registered test runs in its own working directory, so a bare
  // relative name cannot collide with another test's fixture.
  const char *path = "golden.lrpk";
  assert(WriteWholeFile(path, golden));
  StdioByteSource file;
  assert(file.open(loka::core::String::Literal(path)));
  assert(file.isOpen());
  std::size_t fileSize = 0;
  assert(file.size(fileSize) && fileSize == golden.size());

  std::vector<unsigned char> fileIndex;
  std::vector<unsigned char> fileFirstBag;
  std::vector<unsigned char> fileSecondBag;
  Reader fromFile;
  OpenGoldenFromSource(fromFile, file, fileIndex, fileFirstBag, fileSecondBag);
  ExpectGoldenContents(fromFile);
  assert(fileIndex == index);
  assert(fileFirstBag == firstBag);
  assert(fileSecondBag == secondBag);

  // A source asked for bytes past the end of the file answers false rather
  // than short, because the reader has no partial-read concept to receive.
  unsigned char past[4];
  assert(!file.readAt(golden.size() - 2, past, sizeof(past)));
  file.close();
  assert(!file.isOpen());

  std::printf("testLrpkGoldenBytesReadTheSameThroughEveryTransport passed\n");
}
