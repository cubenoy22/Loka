#include "LrpkFormatTests.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

#include "core/resource/lrpk/LrpkReader.hpp"
#include "lrpc/LrpkWriter.hpp"

using namespace loka::core::resource::lrpk;
using loka::lrpc::Writer;

namespace
{
  const U32 kStamp = 0xC0FFEE01UL;

  // Axis 0: depth, enum, one declared value (`@bw`).
  // Axis 1: scale, scalar, baseline 100, declared 200 and 300.
  const std::size_t kAxisDepth = 0;
  const std::size_t kAxisScale = 1;

  void DeclareStandardAxes(Writer &writer)
  {
    const U32 depthValues[1] = {1};
    writer.declareAxis(AXIS_KIND_ENUM, 0, depthValues, 1);
    const U32 scaleValues[2] = {200, 300};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, scaleValues, 2);
  }

  const unsigned char kLogoDefault[] = {'D', 'E', 'F', 'A', 'U', 'L', 'T'};
  const unsigned char kLogoBw[] = {'B', 'W'};
  const unsigned char kLogo2x[] = {'T', 'W', 'O', 'X'};
  const unsigned char kLogo3x[] = {'T', 'H', 'R', 'E', 'E', 'X'};
  const unsigned char kMenuFile[] = {'F', 'i', 'l', 'e'};

  Writer::BuildResult BuildStandardPackage(std::vector<unsigned char> &out, bool withCrc)
  {
    Writer writer;
    DeclareStandardAxes(writer);
    const std::size_t bag = writer.addBag();

    U32 axes[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(42, bag, ASSET_KIND_IMAGE, axes, kLogoDefault, sizeof(kLogoDefault));
    axes[kAxisDepth] = 1;
    writer.addAsset(42, bag, ASSET_KIND_IMAGE, axes, kLogoBw, sizeof(kLogoBw));
    axes[kAxisDepth] = 0;
    axes[kAxisScale] = 1; // 200
    writer.addAsset(42, bag, ASSET_KIND_IMAGE, axes, kLogo2x, sizeof(kLogo2x));
    axes[kAxisScale] = 2; // 300
    writer.addAsset(42, bag, ASSET_KIND_IMAGE, axes, kLogo3x, sizeof(kLogo3x));

    U32 plain[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(43, bag, ASSET_KIND_STRING, plain, kMenuFile, sizeof(kMenuFile));

    return writer.build(kStamp, withCrc, out);
  }

  bool AssetEquals(const Asset &asset, const unsigned char *expected, std::size_t length)
  {
    if (asset.length != length || !asset.bytes)
    {
      return false;
    }
    for (std::size_t i = 0; i < length; ++i)
    {
      if (asset.bytes[i] != expected[i])
      {
        return false;
      }
    }
    return true;
  }

  // Finds the INDX payload so a test can rot exactly one byte of it.
  std::size_t FindChunkPayload(const std::vector<unsigned char> &bytes, U32 fourCC, std::size_t &sizeOut)
  {
    std::size_t cursor = kFixedHeadBytes;
    while (cursor + 8 <= bytes.size())
    {
      const U32 tag = ReadU32BE(&bytes[cursor]);
      const std::size_t payloadSize = static_cast<std::size_t>(ReadU32BE(&bytes[cursor + 4]));
      if (tag == fourCC)
      {
        sizeOut = payloadSize;
        return cursor + 8;
      }
      cursor += 8 + AlignUp(payloadSize, kPayloadAlign);
    }
    sizeOut = 0;
    return bytes.size();
  }
} // namespace

void testLrpkRoundTripsThroughTheIndex()
{
  printf("\n==== [testLrpkRoundTripsThroughTheIndex] start ====\n");

  std::vector<unsigned char> package;
  assert(BuildStandardPackage(package, true) == Writer::BUILD_OK);
  assert(package.size() >= kFixedHeadBytes);
  assert(package.size() % kPayloadAlign == 0 && "payloads align to 4: the 68000 faults on odd-address word access");

  Reader reader;
  assert(reader.openBorrowedBytes(&package[0], package.size(), kStamp) == Reader::OPEN_OK);
  assert(reader.hasCrc());
  assert(reader.bagCount() == 1);
  assert(reader.assetCount() == 5);

  // The bag is not open yet. That is a different failure from "no such id",
  // and the two must not be shown as the same one.
  Facts facts;
  Asset asset;
  assert(reader.get(42, facts, asset) == Reader::GET_BAG_NOT_OPEN);
  assert(reader.get(999, facts, asset) == Reader::GET_NO_SUCH_ID);

  assert(reader.openBag(0) == Reader::BAG_OK);
  assert(reader.isBagOpen(0));

  // No facts at all: every axis is absent, so only the default row survives.
  assert(reader.get(42, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kLogoDefault, sizeof(kLogoDefault)));
  assert(asset.kind == ASSET_KIND_IMAGE);

  assert(reader.get(43, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kMenuFile, sizeof(kMenuFile)));
  assert(asset.kind == ASSET_KIND_STRING);

  // The bytes are served from inside the loaded bag rather than copied out.
  assert(asset.bytes > &package[0] && asset.bytes < &package[0] + package.size());

  reader.closeBag(0);
  assert(reader.get(42, facts, asset) == Reader::GET_BAG_NOT_OPEN);

  printf("==== [testLrpkRoundTripsThroughTheIndex] end ====\n");
}

void testLrpkSelectsRepresentationByAxisKind()
{
  printf("\n==== [testLrpkSelectsRepresentationByAxisKind] start ====\n");

  std::vector<unsigned char> package;
  assert(BuildStandardPackage(package, true) == Writer::BUILD_OK);
  Reader reader;
  assert(reader.openBorrowedBytes(&package[0], package.size(), kStamp) == Reader::OPEN_OK);
  assert(reader.openBag(0) == Reader::BAG_OK);

  Asset asset;

  // Enum axis, fact present and matching: the more specific row wins the tie.
  {
    Facts facts;
    facts.present[kAxisDepth] = true;
    facts.value[kAxisDepth] = 1;
    assert(reader.get(42, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kLogoBw, sizeof(kLogoBw)) && "a 1-bit destination must reach the hand-drawn row");
  }

  // Enum axis, fact absent: a row that writes it cannot be confirmed, so it is
  // dropped rather than matched. This is #189's absent rule as a table rule.
  {
    Facts facts;
    facts.present[kAxisScale] = true;
    facts.value[kAxisScale] = 100;
    assert(reader.get(42, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kLogoDefault, sizeof(kLogoDefault)) &&
           "depth is absent here, so the @bw row must not be selected");
  }

  // Scalar axis: smallest at or above the request. 150% takes the 200 row
  // rather than stretching the 100 baseline, which is what both OSes do.
  {
    Facts facts;
    facts.present[kAxisScale] = true;
    facts.value[kAxisScale] = 150;
    assert(reader.get(42, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kLogo2x, sizeof(kLogo2x)));
  }

  // Exactly 300 takes 300, not 200.
  {
    Facts facts;
    facts.present[kAxisScale] = true;
    facts.value[kAxisScale] = 300;
    assert(reader.get(42, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kLogo3x, sizeof(kLogo3x)));
  }

  // Above every declared value: nothing is at or above, so the largest wins.
  // This clause is what makes selection total.
  {
    Facts facts;
    facts.present[kAxisScale] = true;
    facts.value[kAxisScale] = 400;
    assert(reader.get(42, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kLogo3x, sizeof(kLogo3x)));
  }

  printf("==== [testLrpkSelectsRepresentationByAxisKind] end ====\n");
}

void testLrpkRefusesEveryCheckValueFailure()
{
  printf("\n==== [testLrpkRefusesEveryCheckValueFailure] start ====\n");

  std::vector<unsigned char> good;
  assert(BuildStandardPackage(good, true) == Writer::BUILD_OK);

  Reader reader;

  // Holding another build's package: never waived, because it happens daily
  // and its symptom is silent -- the types still pass, only the screen is wrong.
  assert(reader.openBorrowedBytes(&good[0], good.size(), kStamp + 1) == Reader::OPEN_ID_SPACE_MISMATCH);

  // Truncated or appended: one comparison against the recorded total.
  {
    std::vector<unsigned char> shortened(good.begin(), good.end() - kPayloadAlign);
    assert(reader.openBorrowedBytes(&shortened[0], shortened.size(), kStamp) == Reader::OPEN_TRUNCATED);
  }

  // Index rotted.
  {
    std::vector<unsigned char> rotted(good);
    std::size_t indexSize = 0;
    const std::size_t indexAt = FindChunkPayload(rotted, FourCC('I', 'N', 'D', 'X'), indexSize);
    assert(indexSize > 0);
    rotted[indexAt + indexSize - 1] ^= 0xFF;
    assert(reader.openBorrowedBytes(&rotted[0], rotted.size(), kStamp) == Reader::OPEN_INDEX_CORRUPT);
  }

  // Bag contents rotted: caught when the bag is opened, not at open time,
  // because the check rides along with the read that was happening anyway.
  {
    std::vector<unsigned char> rotted(good);
    std::size_t dataSize = 0;
    const std::size_t dataAt = FindChunkPayload(rotted, FourCC('D', 'A', 'T', 'A'), dataSize);
    assert(dataSize > 0);
    rotted[dataAt] ^= 0xFF;
    assert(reader.openBorrowedBytes(&rotted[0], rotted.size(), kStamp) == Reader::OPEN_OK);
    assert(reader.openBag(0) == Reader::BAG_CONTENTS_CORRUPT);
  }

  // Not a package at all.
  {
    std::vector<unsigned char> junk(kFixedHeadBytes, 0);
    assert(reader.openBorrowedBytes(&junk[0], junk.size(), kStamp) == Reader::OPEN_NOT_A_PACKAGE);
  }

  printf("==== [testLrpkRefusesEveryCheckValueFailure] end ====\n");
}

void testLrpkUnsafeModeOmitsRotButNotIdentity()
{
  printf("\n==== [testLrpkUnsafeModeOmitsRotButNotIdentity] start ====\n");

  std::vector<unsigned char> package;
  assert(BuildStandardPackage(package, false) == Writer::BUILD_OK);

  Reader reader;
  assert(reader.openBorrowedBytes(&package[0], package.size(), kStamp) == Reader::OPEN_OK);
  assert(!reader.hasCrc());
  assert(reader.openBag(0) == Reader::BAG_OK);

  // Rot detection is omitted, so a flipped byte in the bag now passes.
  {
    std::vector<unsigned char> rotted(package);
    std::size_t dataSize = 0;
    const std::size_t dataAt = FindChunkPayload(rotted, FourCC('D', 'A', 'T', 'A'), dataSize);
    rotted[dataAt] ^= 0xFF;
    Reader unsafeReader;
    assert(unsafeReader.openBorrowedBytes(&rotted[0], rotted.size(), kStamp) == Reader::OPEN_OK);
    assert(unsafeReader.openBag(0) == Reader::BAG_OK && "unsafe mode may skip rot detection");
  }

  // Mistaken identity is not omittable, even here.
  {
    Reader unsafeReader;
    assert(unsafeReader.openBorrowedBytes(&package[0], package.size(), kStamp + 1) == Reader::OPEN_ID_SPACE_MISMATCH &&
           "\"might be corrupt\" is allowed; \"might be a different build\" is not");
  }

  printf("==== [testLrpkUnsafeModeOmitsRotButNotIdentity] end ====\n");
}

void testLrpcRefusesPackagesThatWouldMakeSelectionPartial()
{
  printf("\n==== [testLrpcRefusesPackagesThatWouldMakeSelectionPartial] start ====\n");

  // An asset with no axis-free row would make get() able to come up empty.
  {
    Writer writer;
    DeclareStandardAxes(writer);
    const std::size_t bag = writer.addBag();
    U32 axes[kMaxAxes] = {1, 0, 0, 0};
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, axes, kLogoBw, sizeof(kLogoBw));
    std::vector<unsigned char> out;
    assert(writer.build(kStamp, true, out) == Writer::BUILD_ASSET_WITHOUT_DEFAULT_ROW);
  }

  // Two rows with the same (id, bag, axes).
  {
    Writer writer;
    DeclareStandardAxes(writer);
    const std::size_t bag = writer.addBag();
    U32 axes[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, axes, kLogoDefault, sizeof(kLogoDefault));
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, axes, kLogo2x, sizeof(kLogo2x));
    std::vector<unsigned char> out;
    assert(writer.build(kStamp, true, out) == Writer::BUILD_DUPLICATE_ROW);
  }

  // The same id in two different bags is legitimate: that is how an exclusive
  // group such as ja/en carries one string id.
  {
    Writer writer;
    DeclareStandardAxes(writer);
    const std::size_t ja = writer.addBag();
    const std::size_t en = writer.addBag();
    U32 axes[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(100, ja, ASSET_KIND_STRING, axes, kMenuFile, sizeof(kMenuFile));
    writer.addAsset(100, en, ASSET_KIND_STRING, axes, kLogoDefault, sizeof(kLogoDefault));
    std::vector<unsigned char> out;
    assert(writer.build(kStamp, true, out) == Writer::BUILD_OK);

    // Only the open bag's row is a candidate.
    Reader reader;
    assert(reader.openBorrowedBytes(&out[0], out.size(), kStamp) == Reader::OPEN_OK);
    assert(reader.openBag(en) == Reader::BAG_OK);
    Facts facts;
    Asset asset;
    assert(reader.get(100, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kLogoDefault, sizeof(kLogoDefault)) && "the open bag decides which row is served");
  }

  printf("==== [testLrpcRefusesPackagesThatWouldMakeSelectionPartial] end ====\n");
}

void testLrpcRefusesRowsThatWouldNotBeReachable()
{
  printf("\n==== [testLrpcRefusesRowsThatWouldNotBeReachable] start ====\n");

  U32 plain[kMaxAxes] = {0, 0, 0, 0};
  U32 specialized[kMaxAxes] = {1, 0, 0, 0};
  std::vector<unsigned char> out;

  // A default row in one bag does not keep selection total for another bag:
  // only rows in an open bag are candidates, so the specialized-only bag would
  // have nothing to fall back to.
  {
    Writer writer;
    DeclareStandardAxes(writer);
    const std::size_t ja = writer.addBag();
    const std::size_t en = writer.addBag();
    writer.addAsset(100, ja, ASSET_KIND_STRING, plain, kMenuFile, sizeof(kMenuFile));
    writer.addAsset(100, en, ASSET_KIND_STRING, specialized, kLogoBw, sizeof(kLogoBw));
    assert(writer.build(kStamp, true, out) == Writer::BUILD_ASSET_WITHOUT_DEFAULT_ROW &&
           "the default row has to be in the same bag to be reachable");
  }

  // One id, one kind: the row's kind is a redundant field the reader trusts.
  {
    Writer writer;
    DeclareStandardAxes(writer);
    const std::size_t bag = writer.addBag();
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kLogoDefault, sizeof(kLogoDefault));
    writer.addAsset(7, bag, ASSET_KIND_STRING, specialized, kLogoBw, sizeof(kLogoBw));
    assert(writer.build(kStamp, true, out) == Writer::BUILD_ASSET_KIND_MISMATCH);
  }

  // A bag index nobody handed out would produce a row in no bag at all, which
  // the reader can only ever report as "the bag is not open".
  {
    Writer writer;
    DeclareStandardAxes(writer);
    writer.addBag();
    writer.addAsset(7, 4, ASSET_KIND_IMAGE, plain, kLogoDefault, sizeof(kLogoDefault));
    assert(writer.build(kStamp, true, out) == Writer::BUILD_BAD_BAG_REFERENCE);
  }

  printf("==== [testLrpcRefusesRowsThatWouldNotBeReachable] end ====\n");
}

void testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds()
{
  printf("\n==== [testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds] start ====\n");

  std::vector<unsigned char> good;
  assert(BuildStandardPackage(good, false) == Writer::BUILD_OK);
  std::size_t indexSize = 0;
  const std::size_t indexAt = FindChunkPayload(good, FourCC('I', 'N', 'D', 'X'), indexSize);
  assert(indexSize > 0);
  // First bag row starts after bagCount(4) + assetCount(4).
  const std::size_t bagRow = indexAt + 8;

  Reader reader;

  // While the codec is none the bag is not expanded, so a larger expanded size
  // would let a row be bounded against bytes that were never stored -- and the
  // pointer handed back points into the stored payload.
  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[bagRow + 8], ReadU32BE(&bad[bagRow + 8]) + 4096);
    assert(reader.openBorrowedBytes(&bad[0], bad.size(), kStamp) == Reader::OPEN_MALFORMED_INDEX);
  }

  // Offset and size chosen so that their 32-bit sum wraps. Checking by addition
  // would let this through and then CRC an arbitrary region.
  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[bagRow + 0], 0xFFFFFF00UL);
    WriteU32BE(&bad[bagRow + 4], 0x00000200UL);
    WriteU32BE(&bad[bagRow + 8], 0x00000200UL);
    assert(reader.openBorrowedBytes(&bad[0], bad.size(), kStamp) == Reader::OPEN_MALFORMED_INDEX &&
           "bounds must be checked by subtraction, never by adding two untrusted 32-bit fields");
  }

  printf("==== [testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds] end ====\n");
}

void testLrpkRefusesForgedCountsAndUnsortedRows()
{
  printf("\n==== [testLrpkRefusesForgedCountsAndUnsortedRows] start ====\n");

  // The arithmetic rule itself, pinned independently of host word size.
  //
  // A forged package can only demonstrate the wrap on a target whose size_t is
  // 32 bits, so on a 64-bit host the package-level cases below pass with or
  // without the fix and prove nothing about it. These do: each pair of values
  // is one the naive form (`offset + length`, `count * width`) accepts and the
  // subtraction/division form rejects, at any width.
  {
    const std::size_t kMax = ~static_cast<std::size_t>(0);
    assert(!ExtentFits(100, kMax, 2) && "offset + length wraps to 1 here, which a naive bound accepts");
    assert(!ExtentFits(100, 2, kMax));
    assert(ExtentFits(100, 40, 60) && "an extent that exactly fills the space is still inside it");
    assert(!ExtentFits(100, 40, 61));
    assert(!ProductFits(100, (kMax / 16) + 1, 16) && "count * width wraps to 0 here");
    assert(ProductFits(100, 6, 16));
    assert(!ProductFits(100, 7, 16));
    assert(ProductFits(0, 0, 16));
  }

  std::vector<unsigned char> good;
  assert(BuildStandardPackage(good, false) == Writer::BUILD_OK);
  std::size_t indexSize = 0;
  const std::size_t indexAt = FindChunkPayload(good, FourCC('I', 'N', 'D', 'X'), indexSize);
  assert(indexSize > 0);

  Reader reader;

  // A forged assetCount whose product with the 16-byte row width wraps to
  // zero. Sizing the table by multiplication would accept this and then read
  // far outside the buffer on the first lookup.
  {
    std::vector<unsigned char> bad(good);
    // The count is stated twice -- in HEAD and in INDX -- and the reader
    // requires them to agree, so a forgery has to change both.
    WriteU32BE(&bad[indexAt + 4], 0x10000000UL);
    WriteU32BE(&bad[16 + 20], 0x10000000UL);
    assert(reader.openBorrowedBytes(&bad[0], bad.size(), kStamp) == Reader::OPEN_MALFORMED_INDEX &&
           "a declared count must be checked by division, never by multiplying it out first");
  }

  // Ascending id is an invariant get() depends on. An unsorted table would
  // make the binary search answer GET_NO_SUCH_ID for an id that is present,
  // which is a lie rather than a refusal.
  {
    std::vector<unsigned char> bad(good);
    const std::size_t rowsAt = indexAt + 8 + 1 * kBagRowBytes;
    // The standard package has ids 42 x4 then 43. Make the last row sort before
    // the first without changing anything else.
    WriteU32BE(&bad[rowsAt + 4 * kAssetRowBytes], 1);
    assert(reader.openBorrowedBytes(&bad[0], bad.size(), kStamp) == Reader::OPEN_MALFORMED_INDEX);
  }

  printf("==== [testLrpkRefusesForgedCountsAndUnsortedRows] end ====\n");
}

void testLrpcValidatesBeforeItPacks()
{
  printf("\n==== [testLrpcValidatesBeforeItPacks] start ====\n");

  U32 plain[kMaxAxes] = {0, 0, 0, 0};
  std::vector<unsigned char> out;

  // An out-of-range axis index must be refused, not masked. Masking 16 to 0
  // would turn a specialized row into something indistinguishable from the
  // default row, defeating the wall that keeps selection total.
  {
    Writer writer;
    DeclareStandardAxes(writer);
    const std::size_t bag = writer.addBag();
    U32 outOfRange[kMaxAxes] = {16, 0, 0, 0};
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kLogoDefault, sizeof(kLogoDefault));
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, outOfRange, kLogoBw, sizeof(kLogoBw));
    assert(writer.build(kStamp, true, out) == Writer::BUILD_BAD_AXIS_REFERENCE);
  }

  // A declared axis value that does not fit the encoded field must be refused
  // rather than truncated: 65536 would become 0 and select the wrong row.
  {
    Writer writer;
    const U32 huge[1] = {65536};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, huge, 1);
    const std::size_t bag = writer.addBag();
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kLogoDefault, sizeof(kLogoDefault));
    assert(writer.build(kStamp, true, out) == Writer::BUILD_AXIS_VALUE_OUT_OF_RANGE);
  }

  // build() is failure-atomic: a refusal must not destroy a good package the
  // caller already had in the destination vector.
  {
    std::vector<unsigned char> reused;
    assert(BuildStandardPackage(reused, true) == Writer::BUILD_OK);
    const std::size_t goodSize = reused.size();

    Writer writer;
    DeclareStandardAxes(writer);
    writer.addBag();
    writer.addAsset(7, 9, ASSET_KIND_IMAGE, plain, kLogoDefault, sizeof(kLogoDefault));
    assert(writer.build(kStamp, true, reused) == Writer::BUILD_BAD_BAG_REFERENCE);
    assert(reused.size() == goodSize && "a refusal must leave the destination untouched");

    Reader reader;
    assert(reader.openBorrowedBytes(&reused[0], reused.size(), kStamp) == Reader::OPEN_OK &&
           "the package that was already there must still open");
  }

  printf("==== [testLrpcValidatesBeforeItPacks] end ====\n");
}

void testLrpkReaderKeepsItsPackageWhenAReloadIsRefused()
{
  printf("\n==== [testLrpkReaderKeepsItsPackageWhenAReloadIsRefused] start ====\n");

  std::vector<unsigned char> good;
  assert(BuildStandardPackage(good, true) == Writer::BUILD_OK);

  Reader reader;
  assert(reader.openBorrowedBytes(&good[0], good.size(), kStamp) == Reader::OPEN_OK);
  assert(reader.openBag(0) == Reader::BAG_OK);

  // A refused reload must leave the live package alone -- including which bags
  // are open, which is state the caller cannot reconstruct.
  std::vector<unsigned char> junk(kFixedHeadBytes, 0);
  assert(reader.openBorrowedBytes(&junk[0], junk.size(), kStamp) == Reader::OPEN_NOT_A_PACKAGE);
  assert(reader.isOpen() && "a refused reload must not close the reader");
  assert(reader.isBagOpen(0) && "nor silently close its bags");

  Facts facts;
  Asset asset;
  assert(reader.get(42, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kLogoDefault, sizeof(kLogoDefault)));

  // And a refusal that happens late in validation behaves the same way.
  std::vector<unsigned char> wrongStamp(good);
  assert(reader.openBorrowedBytes(&wrongStamp[0], wrongStamp.size(), kStamp + 1) == Reader::OPEN_ID_SPACE_MISMATCH);
  assert(reader.isBagOpen(0));
  assert(reader.get(42, facts, asset) == Reader::GET_OK);

  printf("==== [testLrpkReaderKeepsItsPackageWhenAReloadIsRefused] end ====\n");
}

void testLrpkChecksTheChunkThatDecidesSelection()
{
  printf("\n==== [testLrpkChecksTheChunkThatDecidesSelection] start ====\n");

  std::vector<unsigned char> good;
  assert(BuildStandardPackage(good, true) == Writer::BUILD_OK);
  std::size_t axesSize = 0;
  const std::size_t axesAt = FindChunkPayload(good, FourCC('A', 'X', 'E', 'S'), axesSize);
  assert(axesSize > 0);

  Reader reader;

  // AXES carries the kinds, baselines and scalar thresholds that decide which
  // representation is served. Leaving it outside the checked metadata would let
  // a bit flip there change the picture while every recorded CRC still matched
  // -- the silent class of failure the check values exist to prevent.
  {
    std::vector<unsigned char> rotted(good);
    rotted[axesAt + 8] ^= 0xFF; // a scalar baseline
    assert(reader.openBorrowedBytes(&rotted[0], rotted.size(), kStamp) == Reader::OPEN_INDEX_CORRUPT);
  }

  // An axis kind this version does not know is refused rather than carried.
  // Both the enum filter and the scalar ranking skip an unknown kind, so a row
  // writing that axis could win on the specificity tie-break alone while
  // matching nothing.
  {
    std::vector<unsigned char> bad(good);
    bad[axesAt + 4] = 9; // first axis entry, kind byte
    // Re-stamp the AXES CRC so the kind check is what refuses this, not the CRC.
    Crc32 crc;
    crc.update(&bad[axesAt], axesSize);
    WriteU32BE(&bad[16 + 28], crc.value());
    assert(reader.openBorrowedBytes(&bad[0], bad.size(), kStamp) == Reader::OPEN_MALFORMED_INDEX);
  }

  printf("==== [testLrpkChecksTheChunkThatDecidesSelection] end ====\n");
}
