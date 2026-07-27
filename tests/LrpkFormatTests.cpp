#include "LrpkFormatTests.hpp"

#include <algorithm>
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
  const std::size_t kAxisDepth = 0;
  const std::size_t kAxisAppearance = 0;
  const std::size_t kAxisScale = 1;

  const unsigned char kDefault[] = {'D', 'E', 'F', 'A', 'U', 'L', 'T'};
  const unsigned char kBw[] = {'B', 'W'};
  const unsigned char k2x[] = {'T', 'W', 'O', 'X'};
  const unsigned char k3x[] = {'T', 'H', 'R', 'E', 'E', 'X'};
  const unsigned char kDark[] = {'D', 'A', 'R', 'K'};
  const unsigned char kFile[] = {'F', 'i', 'l', 'e'};

  void DeclareDepthScale(Writer &writer, bool depthFirst)
  {
    const U32 depthValues[1] = {1};
    writer.declareAxis(AXIS_KIND_ENUM, 0, depthValues, 1);
    const U32 scaleValues[2] = {200, 300};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, scaleValues, 2);
    const std::size_t depthThenScale[2] = {kAxisDepth, kAxisScale};
    const std::size_t scaleThenDepth[2] = {kAxisScale, kAxisDepth};
    writer.setRepresentationPrecedence(depthFirst ? depthThenScale : scaleThenDepth, 2);
  }

  Writer::BuildResult BuildDepthScalePackage(std::vector<unsigned char> &out,
                                             bool depthFirst)
  {
    Writer writer;
    DeclareDepthScale(writer, depthFirst);
    const std::size_t bag = writer.addBag();
    U32 axes[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(42, bag, ASSET_KIND_IMAGE, axes, kDefault, sizeof(kDefault));
    axes[kAxisDepth] = 1;
    writer.addAsset(42, bag, ASSET_KIND_IMAGE, axes, kBw, sizeof(kBw));
    axes[kAxisDepth] = 0;
    axes[kAxisScale] = 1;
    writer.addAsset(42, bag, ASSET_KIND_IMAGE, axes, k2x, sizeof(k2x));
    axes[kAxisScale] = 2;
    writer.addAsset(42, bag, ASSET_KIND_IMAGE, axes, k3x, sizeof(k3x));
    U32 plain[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(43, bag, ASSET_KIND_STRING, plain, kFile, sizeof(kFile));
    return writer.build(kStamp, out);
  }

  Writer::BuildResult BuildAppearanceScalePackage(std::vector<unsigned char> &out,
                                                  bool appearanceFirst)
  {
    Writer writer;
    const U32 darkValue[1] = {1};
    writer.declareAxis(AXIS_KIND_ENUM, 0, darkValue, 1);
    const U32 scaleValue[1] = {200};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, scaleValue, 1);
    const std::size_t appearanceThenScale[2] = {kAxisAppearance, kAxisScale};
    const std::size_t scaleThenAppearance[2] = {kAxisScale, kAxisAppearance};
    writer.setRepresentationPrecedence(appearanceFirst ? appearanceThenScale : scaleThenAppearance, 2);
    const std::size_t bag = writer.addBag();
    U32 axes[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, axes, kDefault, sizeof(kDefault));
    axes[kAxisAppearance] = 1;
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, axes, kDark, sizeof(kDark));
    axes[kAxisAppearance] = 0;
    axes[kAxisScale] = 1;
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, axes, k2x, sizeof(k2x));
    return writer.build(kStamp, out);
  }

  Writer::BuildResult BuildEnumVocabularyPackage(std::vector<unsigned char> &out,
                                                 bool selectedValueFirst)
  {
    Writer writer;
    const U32 valuesForward[2] = {10, 20};
    const U32 valuesReverse[2] = {20, 10};
    writer.declareAxis(AXIS_KIND_ENUM,
                       0,
                       selectedValueFirst ? valuesForward : valuesReverse,
                       2);
    const std::size_t bag = writer.addBag();
    U32 axes[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(81, bag, ASSET_KIND_IMAGE, axes, kDefault, sizeof(kDefault));
    axes[0] = selectedValueFirst ? 1 : 2;
    writer.addAsset(81, bag, ASSET_KIND_IMAGE, axes, kDark, sizeof(kDark));
    return writer.build(kStamp, out);
  }

  bool AssetEquals(const Asset &asset,
                   const unsigned char *expected,
                   std::size_t length)
  {
    if (!asset.bytes || asset.length != length)
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

  std::size_t FindChunkHeader(const std::vector<unsigned char> &bytes,
                              U32 tag,
                              std::size_t &payloadSize)
  {
    std::size_t cursor = kFixedHeadBytes;
    while (cursor < bytes.size())
    {
      assert(bytes.size() - cursor >= kChunkHeaderBytes);
      const std::size_t size =
          static_cast<std::size_t>(ReadU32BE(&bytes[cursor + 4]));
      if (ReadU32BE(&bytes[cursor]) == tag)
      {
        payloadSize = size;
        return cursor;
      }
      cursor += kChunkHeaderBytes + AlignUp(size, kPayloadAlign);
    }
    payloadSize = 0;
    return bytes.size();
  }

  std::size_t FindChunkPayload(const std::vector<unsigned char> &bytes,
                               U32 tag,
                               std::size_t &payloadSize)
  {
    return FindChunkHeader(bytes, tag, payloadSize) + kChunkHeaderBytes;
  }

  void RestampHead(std::vector<unsigned char> &bytes)
  {
    Crc32 crc;
    crc.update(&bytes[kFormHeaderBytes], kChunkHeaderBytes);
    crc.update(&bytes[kHeadPayloadOffset + 4], kHeadPayloadBytes - 4);
    WriteU32BE(&bytes[kHeadPayloadOffset + kHeadCrc], crc.value());
  }

  void RestampChunk(std::vector<unsigned char> &bytes,
                    U32 tag,
                    std::size_t headCrcField)
  {
    std::size_t payloadSize = 0;
    const std::size_t header = FindChunkHeader(bytes, tag, payloadSize);
    assert(header < bytes.size());
    WriteU32BE(&bytes[kHeadPayloadOffset + headCrcField],
               Crc32::Of(&bytes[header], kChunkHeaderBytes + payloadSize));
    RestampHead(bytes);
  }

  void DuplicateFirstAssetRow(std::vector<unsigned char> &bytes)
  {
    std::size_t indexSize = 0;
    const std::size_t indexHeader =
        FindChunkHeader(bytes, FourCC('I', 'N', 'D', 'X'), indexSize);
    const std::size_t indexPayload = indexHeader + kChunkHeaderBytes;
    const std::size_t bagCount =
        static_cast<std::size_t>(ReadU32BE(&bytes[indexPayload]));
    const std::size_t rowsAt =
        indexPayload + 8 + bagCount * kBagRowBytes;
    std::vector<unsigned char> duplicate(
        bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(rowsAt),
        bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(rowsAt +
                                                                                 kAssetRowBytes));
    bytes.insert(bytes.begin() +
                     static_cast<std::vector<unsigned char>::difference_type>(rowsAt +
                                                                              kAssetRowBytes),
                 duplicate.begin(),
                 duplicate.end());
    WriteU32BE(&bytes[indexHeader + 4],
               static_cast<U32>(indexSize + kAssetRowBytes));
    WriteU32BE(&bytes[indexPayload + 4],
               ReadU32BE(&bytes[indexPayload + 4]) + 1);
    WriteU32BE(&bytes[kHeadPayloadOffset + kHeadAssetCount],
               ReadU32BE(&bytes[kHeadPayloadOffset + kHeadAssetCount]) + 1);
    WriteU32BE(&bytes[4],
               static_cast<U32>(bytes.size() - kFormHeaderBytes));
    WriteU32BE(&bytes[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(bytes.size()));
    RestampChunk(bytes, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);
  }

  void OpenOneBag(Reader &reader,
                  const std::vector<unsigned char> &package)
  {
    assert(reader.openBorrowedBytes(&package[0],
                                    package.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
    assert(reader.openBag(0) == Reader::BAG_OK);
  }

  void ExpectSelection(const std::vector<unsigned char> &package,
                       const Facts &facts,
                       const unsigned char *bytes,
                       std::size_t length)
  {
    Reader reader;
    OpenOneBag(reader, package);
    Asset asset;
    assert(reader.get(42, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, bytes, length));
  }
} // namespace

void testLrpkRoundTripsThroughTheIndex()
{
  printf("\n==== [testLrpkRoundTripsThroughTheIndex] start ====\n");

  std::vector<unsigned char> package;
  assert(BuildDepthScalePackage(package, true) == Writer::BUILD_OK);
  assert(package.size() % kPayloadAlign == 0);

  // Byte-level writer/reader cross-check for the ruled HEAD and AXES layout.
  const unsigned char *head = &package[kHeadPayloadOffset];
  assert(ReadU32BE(&package[0]) == FourCC('L', 'R', 'P', 'K'));
  assert(ReadU32BE(&package[kFormHeaderBytes]) == FourCC('H', 'E', 'A', 'D'));
  assert(ReadU32BE(head + kHeadCrc) != 0 && "headCrc is the first HEAD field");
  assert(ReadU32BE(head + kHeadVersion) == kFormatVersion);
  assert(ReadU32BE(head + kHeadTotalBytes) == package.size());
  assert(ReadU32BE(head + kHeadFlags) == 0 && "HEAD carries no CRC-skip flag");
  assert(ReadU32BE(head + kHeadAssetCount) == 5);
  assert(ReadU32BE(head + kHeadBagCount) == 1);

  Crc32 headCheck;
  headCheck.update(&package[kFormHeaderBytes], kChunkHeaderBytes);
  headCheck.update(&package[kHeadPayloadOffset + 4], kHeadPayloadBytes - 4);
  assert(headCheck.value() == ReadU32BE(head + kHeadCrc));

  std::size_t axesSize = 0;
  const std::size_t axesHeader =
      FindChunkHeader(package, FourCC('A', 'X', 'E', 'S'), axesSize);
  const std::size_t axesPayload = axesHeader + kChunkHeaderBytes;
  assert(package[axesPayload] == 2);
  assert(package[axesPayload + 4 + kAxisPrecedenceRank] == 0);
  assert(package[axesPayload + 4 + kAxisEntryBytes + kAxisPrecedenceRank] == 1);
  assert(Crc32::Of(&package[axesHeader], kChunkHeaderBytes + axesSize) ==
         ReadU32BE(head + kHeadAxesCrc));

  std::size_t indexSize = 0;
  const std::size_t indexHeader =
      FindChunkHeader(package, FourCC('I', 'N', 'D', 'X'), indexSize);
  assert(Crc32::Of(&package[indexHeader], kChunkHeaderBytes + indexSize) ==
         ReadU32BE(head + kHeadIndexCrc));
  std::size_t dataSize = 0;
  const std::size_t dataHeader =
      FindChunkHeader(package, FourCC('D', 'A', 'T', 'A'), dataSize);
  assert(Crc32::Of(&package[dataHeader], kChunkHeaderBytes) ==
         ReadU32BE(head + kHeadDataHeaderCrc));

  Reader reader;
  assert(reader.openBorrowedBytes(&package[0],
                                  package.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  assert(reader.verifiesIntegrity());
  assert(reader.bagCount() == 1);
  assert(reader.assetCount() == 5);

  Facts facts;
  Asset asset;
  assert(reader.get(42, facts, asset) == Reader::GET_BAG_NOT_OPEN);
  assert(reader.get(999, facts, asset) == Reader::GET_NO_SUCH_ID);
  assert(reader.openBag(0) == Reader::BAG_OK);
  assert(reader.get(42, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kDefault, sizeof(kDefault)));
  assert(asset.kind == ASSET_KIND_IMAGE);
  assert(reader.get(43, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kFile, sizeof(kFile)));
  assert(asset.bytes > &package[0] &&
         asset.bytes < &package[0] + package.size());
  reader.closeBag(0);
  assert(reader.get(42, facts, asset) == Reader::GET_BAG_NOT_OPEN);

  printf("==== [testLrpkRoundTripsThroughTheIndex] end ====\n");
}

void testLrpkSelectsByPackagePrecedence()
{
  printf("\n==== [testLrpkSelectsByPackagePrecedence] start ====\n");

  std::vector<unsigned char> depthFirst;
  std::vector<unsigned char> scaleFirst;
  assert(BuildDepthScalePackage(depthFirst, true) == Writer::BUILD_OK);
  assert(BuildDepthScalePackage(scaleFirst, false) == Writer::BUILD_OK);

  // Frozen acceptance truth table: depth=bw, scale=150.
  Facts bw150;
  bw150.present[kAxisDepth] = true;
  bw150.value[kAxisDepth] = 1;
  bw150.present[kAxisScale] = true;
  bw150.value[kAxisScale] = 150;
  ExpectSelection(depthFirst, bw150, kBw, sizeof(kBw));
  ExpectSelection(scaleFirst, bw150, k2x, sizeof(k2x));

  // depth=4 removes @bw during eligibility before precedence runs.
  Facts depth4Scale150;
  depth4Scale150.present[kAxisDepth] = true;
  depth4Scale150.value[kAxisDepth] = 4;
  depth4Scale150.present[kAxisScale] = true;
  depth4Scale150.value[kAxisScale] = 150;
  ExpectSelection(depthFirst, depth4Scale150, k2x, sizeof(k2x));

  // Missing facts never become baselines. Written rows on those axes drop.
  Facts scale150;
  scale150.present[kAxisScale] = true;
  scale150.value[kAxisScale] = 150;
  ExpectSelection(depthFirst, scale150, k2x, sizeof(k2x));

  Facts bwOnly;
  bwOnly.present[kAxisDepth] = true;
  bwOnly.value[kAxisDepth] = 1;
  ExpectSelection(depthFirst, bwOnly, kBw, sizeof(kBw));

  Facts absent;
  ExpectSelection(depthFirst, absent, kDefault, sizeof(kDefault));

  Facts staleAbsent;
  staleAbsent.value[kAxisScale] = 200;
  ExpectSelection(depthFirst, staleAbsent, kDefault, sizeof(kDefault));

  // Scalar-only rows: ceiling tier, largest fallback, and the baseline.
  Facts scale250;
  scale250.present[kAxisScale] = true;
  scale250.value[kAxisScale] = 250;
  ExpectSelection(depthFirst, scale250, k3x, sizeof(k3x));
  Facts scale350;
  scale350.present[kAxisScale] = true;
  scale350.value[kAxisScale] = 350;
  ExpectSelection(depthFirst, scale350, k3x, sizeof(k3x));
  Facts scale75;
  scale75.present[kAxisScale] = true;
  scale75.value[kAxisScale] = 75;
  ExpectSelection(depthFirst, scale75, kDefault, sizeof(kDefault));

  // Appearance versus scale uses the same package-owned policy mechanism.
  std::vector<unsigned char> appearanceFirst;
  std::vector<unsigned char> appearanceScaleFirst;
  assert(BuildAppearanceScalePackage(appearanceFirst, true) == Writer::BUILD_OK);
  assert(BuildAppearanceScalePackage(appearanceScaleFirst, false) == Writer::BUILD_OK);
  Facts dark150;
  dark150.present[kAxisAppearance] = true;
  dark150.value[kAxisAppearance] = 1;
  dark150.present[kAxisScale] = true;
  dark150.value[kAxisScale] = 150;
  {
    Reader reader;
    OpenOneBag(reader, appearanceFirst);
    Asset asset;
    assert(reader.get(7, dark150, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDark, sizeof(kDark)));
  }
  {
    Reader reader;
    OpenOneBag(reader, appearanceScaleFirst);
    Asset asset;
    assert(reader.get(7, dark150, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, k2x, sizeof(k2x)));
  }

  // Enum facts carry declared vocabulary values, not their physical slots.
  // Reordering the vocabulary must not change the meaning of a caller fact.
  {
    std::vector<unsigned char> selectedValueFirst;
    std::vector<unsigned char> selectedValueSecond;
    assert(BuildEnumVocabularyPackage(selectedValueFirst, true) == Writer::BUILD_OK);
    assert(BuildEnumVocabularyPackage(selectedValueSecond, false) == Writer::BUILD_OK);
    Facts declaredTen;
    declaredTen.present[0] = true;
    declaredTen.value[0] = 10;
    Reader reader;
    OpenOneBag(reader, selectedValueFirst);
    Asset asset;
    assert(reader.get(81, declaredTen, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDark, sizeof(kDark)));
    OpenOneBag(reader, selectedValueSecond);
    assert(reader.get(81, declaredTen, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDark, sizeof(kDark)));
  }

  // Physical row order is not semantic. Exercise all 4! orders of this id's
  // representation run, re-stamp each chunk, and require the same answer.
  {
    std::size_t indexSize = 0;
    const std::size_t indexAt =
        FindChunkPayload(depthFirst, FourCC('I', 'N', 'D', 'X'), indexSize);
    const std::size_t rowsAt = indexAt + 8 + kBagRowBytes;
    std::size_t order[4] = {0, 1, 2, 3};
    do
    {
      std::vector<unsigned char> permuted(depthFirst);
      for (std::size_t destination = 0; destination < 4; ++destination)
      {
        for (std::size_t byte = 0; byte < kAssetRowBytes; ++byte)
        {
          permuted[rowsAt + destination * kAssetRowBytes + byte] =
              depthFirst[rowsAt + order[destination] * kAssetRowBytes + byte];
        }
      }
      RestampChunk(permuted, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);
      ExpectSelection(permuted, bw150, kBw, sizeof(kBw));
    } while (std::next_permutation(order, order + 4));
  }

  // A forged package can bypass lrpc's ambiguity wall. Byte-identical rows
  // remain indistinguishable after Phase B and must be a typed refusal.
  {
    std::vector<unsigned char> duplicateRows(depthFirst);
    DuplicateFirstAssetRow(duplicateRows);
    Reader reader;
    OpenOneBag(reader, duplicateRows);
    Asset asset;
    Facts noFacts;
    assert(reader.get(42, noFacts, asset) == Reader::GET_NO_MATCHING_REP);
  }

  printf("==== [testLrpkSelectsByPackagePrecedence] end ====\n");
}

void testLrpkRefusesEveryCheckValueFailure()
{
  printf("\n==== [testLrpkRefusesEveryCheckValueFailure] start ====\n");

  std::vector<unsigned char> good;
  assert(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  Reader reader;
  assert(reader.openBorrowedBytes(&good[0],
                                  good.size(),
                                  kStamp + 1,
                                  Reader::VERIFY_INTEGRITY) ==
         Reader::OPEN_ID_SPACE_MISMATCH);

  {
    std::vector<unsigned char> shortened(good.begin(),
                                         good.end() - kPayloadAlign);
    assert(reader.openBorrowedBytes(&shortened[0],
                                    shortened.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_TRUNCATED);
  }
  {
    std::vector<unsigned char> rotted(good);
    std::size_t indexSize = 0;
    const std::size_t indexAt =
        FindChunkPayload(rotted, FourCC('I', 'N', 'D', 'X'), indexSize);
    rotted[indexAt + indexSize - 1] ^= 0xFF;
    assert(reader.openBorrowedBytes(&rotted[0],
                                    rotted.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_INDEX_CORRUPT);
  }
  {
    std::vector<unsigned char> rotted(good);
    std::size_t dataSize = 0;
    const std::size_t dataAt =
        FindChunkPayload(rotted, FourCC('D', 'A', 'T', 'A'), dataSize);
    rotted[dataAt] ^= 0xFF;
    assert(reader.openBorrowedBytes(&rotted[0],
                                    rotted.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    assert(reader.openBag(0) == Reader::BAG_CONTENTS_CORRUPT);
  }
  {
    std::vector<unsigned char> junk(kFixedHeadBytes, 0);
    assert(reader.openBorrowedBytes(&junk[0],
                                    junk.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_NOT_A_PACKAGE);
  }

  printf("==== [testLrpkRefusesEveryCheckValueFailure] end ====\n");
}

void testLrpkOpenControlsIntegrityVerification()
{
  printf("\n==== [testLrpkOpenControlsIntegrityVerification] start ====\n");

  std::vector<unsigned char> package;
  assert(BuildDepthScalePackage(package, true) == Writer::BUILD_OK);
  std::vector<unsigned char> rotted(package);
  std::size_t dataSize = 0;
  const std::size_t dataAt =
      FindChunkPayload(rotted, FourCC('D', 'A', 'T', 'A'), dataSize);
  rotted[dataAt] ^= 0xFF;

  Reader unchecked;
  assert(unchecked.openBorrowedBytes(&rotted[0],
                                     rotted.size(),
                                     kStamp,
                                     Reader::SKIP_INTEGRITY) ==
         Reader::OPEN_OK);
  assert(!unchecked.verifiesIntegrity());
  assert(unchecked.openBag(0) == Reader::BAG_OK);

  Reader wrongBuild;
  assert(wrongBuild.openBorrowedBytes(&package[0],
                                      package.size(),
                                      kStamp + 1,
                                      Reader::SKIP_INTEGRITY) ==
         Reader::OPEN_ID_SPACE_MISMATCH);

  printf("==== [testLrpkOpenControlsIntegrityVerification] end ====\n");
}

void testLrpcRefusesPackagesThatWouldMakeSelectionPartial()
{
  printf("\n==== [testLrpcRefusesPackagesThatWouldMakeSelectionPartial] start ====\n");

  U32 plain[kMaxAxes] = {0, 0, 0, 0};
  U32 specialized[kMaxAxes] = {1, 0, 0, 0};
  std::vector<unsigned char> out;

  {
    Writer writer;
    DeclareDepthScale(writer, true);
    const std::size_t bag = writer.addBag();
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, specialized, kBw, sizeof(kBw));
    assert(writer.build(kStamp, out) ==
           Writer::BUILD_ASSET_WITHOUT_DEFAULT_ROW);
  }
  {
    Writer writer;
    DeclareDepthScale(writer, true);
    const std::size_t bag = writer.addBag();
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, k2x, sizeof(k2x));
    assert(writer.build(kStamp, out) ==
           Writer::BUILD_SELECTOR_AMBIGUOUS);
  }

  // Duplicate ids across bags are valid package data, but the bags are not
  // co-openable.
  {
    Writer writer;
    const std::size_t ja = writer.addBag();
    const std::size_t en = writer.addBag();
    writer.addAsset(100, ja, ASSET_KIND_STRING, plain, kFile, sizeof(kFile));
    writer.addAsset(100, en, ASSET_KIND_STRING, plain, kDefault, sizeof(kDefault));
    assert(writer.build(kStamp, out) == Writer::BUILD_OK);
    Reader reader;
    assert(reader.openBorrowedBytes(&out[0],
                                    out.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    assert(reader.openBag(ja) == Reader::BAG_OK);
    assert(reader.openBag(en) == Reader::BAG_ASSET_ID_CONFLICT);
  }

  printf("==== [testLrpcRefusesPackagesThatWouldMakeSelectionPartial] end ====\n");
}

void testLrpcRefusesRowsThatWouldNotBeReachable()
{
  printf("\n==== [testLrpcRefusesRowsThatWouldNotBeReachable] start ====\n");

  U32 plain[kMaxAxes] = {0, 0, 0, 0};
  U32 specialized[kMaxAxes] = {1, 0, 0, 0};
  std::vector<unsigned char> out;

  {
    Writer writer;
    DeclareDepthScale(writer, true);
    const std::size_t ja = writer.addBag();
    const std::size_t en = writer.addBag();
    writer.addAsset(100, ja, ASSET_KIND_STRING, plain, kFile, sizeof(kFile));
    writer.addAsset(100, en, ASSET_KIND_STRING, specialized, kBw, sizeof(kBw));
    assert(writer.build(kStamp, out) ==
           Writer::BUILD_ASSET_WITHOUT_DEFAULT_ROW);
  }
  {
    Writer writer;
    DeclareDepthScale(writer, true);
    const std::size_t bag = writer.addBag();
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    writer.addAsset(7, bag, ASSET_KIND_STRING, specialized, kBw, sizeof(kBw));
    assert(writer.build(kStamp, out) ==
           Writer::BUILD_ASSET_KIND_MISMATCH);
  }
  {
    Writer writer;
    DeclareDepthScale(writer, true);
    writer.addBag();
    writer.addAsset(7, 4, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(writer.build(kStamp, out) == Writer::BUILD_BAD_BAG_REFERENCE);
  }
  {
    Writer writer;
    const U32 depth[1] = {1};
    writer.declareAxis(AXIS_KIND_ENUM, 0, depth, 1);
    const std::size_t bag = writer.addBag();
    U32 undeclared[kMaxAxes] = {0, 1, 0, 0};
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, undeclared, kDefault, sizeof(kDefault));
    assert(writer.build(kStamp, out) == Writer::BUILD_BAD_AXIS_REFERENCE &&
           "an index for an undeclared axis must not be dropped");
  }
  {
    Writer writer;
    const U32 scale[1] = {100};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, scale, 1);
    const std::size_t bag = writer.addBag();
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    U32 explicitBaseline[kMaxAxes] = {1, 0, 0, 0};
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, explicitBaseline, k2x, sizeof(k2x));
    assert(writer.build(kStamp, out) ==
           Writer::BUILD_SCALAR_BASELINE_EXPLICIT);
  }

  // Two or more axes require an exact package policy permutation.
  {
    Writer missing;
    const U32 one[1] = {1};
    missing.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    missing.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    const std::size_t bag = missing.addBag();
    missing.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(missing.build(kStamp, out) == Writer::BUILD_BAD_PRECEDENCE);
  }
  {
    Writer duplicate;
    const U32 one[1] = {1};
    duplicate.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    duplicate.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    const std::size_t bad[2] = {0, 0};
    duplicate.setRepresentationPrecedence(bad, 2);
    const std::size_t bag = duplicate.addBag();
    duplicate.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(duplicate.build(kStamp, out) == Writer::BUILD_BAD_PRECEDENCE);
  }
  {
    Writer incomplete;
    const U32 one[1] = {1};
    incomplete.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    incomplete.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    const std::size_t onlyOne[1] = {0};
    incomplete.setRepresentationPrecedence(onlyOne, 1);
    const std::size_t bag = incomplete.addBag();
    incomplete.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(incomplete.build(kStamp, out) == Writer::BUILD_BAD_PRECEDENCE);
  }
  {
    Writer unknown;
    const U32 one[1] = {1};
    unknown.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    unknown.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    const std::size_t bad[2] = {0, 2};
    unknown.setRepresentationPrecedence(bad, 2);
    const std::size_t bag = unknown.addBag();
    unknown.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(unknown.build(kStamp, out) == Writer::BUILD_BAD_PRECEDENCE);
  }

  printf("==== [testLrpcRefusesRowsThatWouldNotBeReachable] end ====\n");
}

void testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds()
{
  printf("\n==== [testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds] start ====\n");

  std::vector<unsigned char> good;
  assert(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  std::size_t indexSize = 0;
  const std::size_t indexAt =
      FindChunkPayload(good, FourCC('I', 'N', 'D', 'X'), indexSize);
  const std::size_t bagRow = indexAt + 8;
  Reader reader;

  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[bagRow + kBagExpandedSize],
               ReadU32BE(&bad[bagRow + kBagExpandedSize]) + 4096);
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[bagRow + kBagDataOffset], 0xFFFFFF00UL);
    WriteU32BE(&bad[bagRow + kBagStoredSize], 0x00000200UL);
    WriteU32BE(&bad[bagRow + kBagExpandedSize], 0x00000200UL);
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }

  printf("==== [testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds] end ====\n");
}

void testLrpkRefusesForgedCountsAndUnsortedRows()
{
  printf("\n==== [testLrpkRefusesForgedCountsAndUnsortedRows] start ====\n");

  {
    const std::size_t maximum = ~static_cast<std::size_t>(0);
    assert(!ExtentFits(100, maximum, 2));
    assert(!ProductFits(100, (maximum / 16) + 1, 16));

    // Pin the narrowing rule as two exact U32 words. This is independent of
    // host word size; it does not claim that a 64-bit run exercised a 32-bit
    // size_t implementation.
    assert(U32WordsFit(0, kU32Mask));
    assert(!U32WordsFit(1, 0));
    assert(SizeFitsU32(65535));
  }

  std::vector<unsigned char> good;
  assert(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  std::size_t indexSize = 0;
  const std::size_t indexAt =
      FindChunkPayload(good, FourCC('I', 'N', 'D', 'X'), indexSize);
  Reader reader;

  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[indexAt + 4], 0x10000000UL);
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadAssetCount], 0x10000000UL);
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    const std::size_t rowsAt = indexAt + 8 + kBagRowBytes;
    WriteU32BE(&bad[rowsAt + 4 * kAssetRowBytes + kRowId], 1);
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    const std::size_t rowsAt = indexAt + 8 + kBagRowBytes;
    bad[rowsAt + kRowBag] = static_cast<unsigned char>(kMaxBags);
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX &&
           "a nonexistent bag is malformed data, not GET_BAG_NOT_OPEN");
  }

  Reader unopened;
  Facts facts;
  Asset asset;
  assert(unopened.get(42, facts, asset) == Reader::GET_BAG_NOT_OPEN);

  printf("==== [testLrpkRefusesForgedCountsAndUnsortedRows] end ====\n");
}

void testLrpcValidatesBeforeItPacks()
{
  printf("\n==== [testLrpcValidatesBeforeItPacks] start ====\n");

  U32 plain[kMaxAxes] = {0, 0, 0, 0};
  std::vector<unsigned char> out;

  {
    Writer writer;
    DeclareDepthScale(writer, true);
    const std::size_t bag = writer.addBag();
    U32 outOfRange[kMaxAxes] = {16, 0, 0, 0};
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, outOfRange, kBw, sizeof(kBw));
    assert(writer.build(kStamp, out) == Writer::BUILD_BAD_AXIS_REFERENCE);
  }
  {
    Writer writer;
    const U32 huge[1] = {65536};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, huge, 1);
    const std::size_t bag = writer.addBag();
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(writer.build(kStamp, out) ==
           Writer::BUILD_AXIS_VALUE_OUT_OF_RANGE);
  }
  // A trailing empty bag used to form &data[data.size()] while computing its
  // zero-length CRC. The package must build and the empty bag must verify.
  {
    Writer writer;
    const std::size_t full = writer.addBag();
    const std::size_t empty = writer.addBag();
    writer.addAsset(7, full, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(writer.build(kStamp, out) == Writer::BUILD_OK);
    Reader reader;
    assert(reader.openBorrowedBytes(&out[0],
                                    out.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    assert(reader.openBag(empty) == Reader::BAG_OK);
  }

  // kMaxBags has one named home shared by storage and validation.
  {
    Writer writer;
    for (std::size_t i = 0; i < kMaxBags; ++i)
    {
      writer.addBag();
    }
    writer.addAsset(7, 0, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(writer.build(kStamp, out) == Writer::BUILD_OK);
    writer.addBag();
    assert(writer.build(kStamp, out) == Writer::BUILD_TOO_MANY_BAGS);
  }

  {
    std::vector<unsigned char> reused;
    assert(BuildDepthScalePackage(reused, true) == Writer::BUILD_OK);
    const std::vector<unsigned char> before(reused);
    Writer writer;
    writer.addBag();
    writer.addAsset(7, 9, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    assert(writer.build(kStamp, reused) == Writer::BUILD_BAD_BAG_REFERENCE);
    assert(reused == before);
  }

  printf("==== [testLrpcValidatesBeforeItPacks] end ====\n");
}

void testLrpkReaderKeepsItsPackageWhenAReloadIsRefused()
{
  printf("\n==== [testLrpkReaderKeepsItsPackageWhenAReloadIsRefused] start ====\n");

  std::vector<unsigned char> good;
  assert(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  Reader reader;
  OpenOneBag(reader, good);

  std::vector<unsigned char> junk(kFixedHeadBytes, 0);
  assert(reader.openBorrowedBytes(&junk[0],
                                  junk.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) ==
         Reader::OPEN_NOT_A_PACKAGE);
  assert(reader.isOpen());
  assert(reader.isBagOpen(0));
  Facts facts;
  Asset asset;
  assert(reader.get(42, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kDefault, sizeof(kDefault)));

  assert(reader.openBorrowedBytes(&good[0],
                                  good.size(),
                                  kStamp + 1,
                                  Reader::VERIFY_INTEGRITY) ==
         Reader::OPEN_ID_SPACE_MISMATCH);
  assert(reader.isBagOpen(0));

  printf("==== [testLrpkReaderKeepsItsPackageWhenAReloadIsRefused] end ====\n");
}

void testLrpkChecksTheChunkThatDecidesSelection()
{
  printf("\n==== [testLrpkChecksTheChunkThatDecidesSelection] start ====\n");

  std::vector<unsigned char> good;
  assert(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  std::size_t axesSize = 0;
  const std::size_t axesAt =
      FindChunkPayload(good, FourCC('A', 'X', 'E', 'S'), axesSize);
  Reader reader;

  {
    std::vector<unsigned char> rotted(good);
    rotted[axesAt + 4 + kAxisBaseline] ^= 0xFF;
    assert(reader.openBorrowedBytes(&rotted[0],
                                    rotted.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_INDEX_CORRUPT);
  }
  {
    std::vector<unsigned char> bad(good);
    bad[axesAt + 4 + kAxisKind] = 9;
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    bad[axesAt + 4 + kAxisEntryBytes + kAxisPrecedenceRank] = 0;
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX &&
           "precedence is structural even when CRC verification is skipped");
  }
  {
    std::vector<unsigned char> bad(good);
    bad[kHeadPayloadOffset + kHeadFlags + 3] = 1;
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_HEAD_CORRUPT);
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    std::size_t ignored = 0;
    const std::size_t axesHeader =
        FindChunkHeader(bad, FourCC('A', 'X', 'E', 'S'), ignored);
    bad[axesHeader] = 'a';
    assert(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_UNKNOWN_CHUNK);
  }

  // AXES is required even at axisCount==0.
  {
    Writer writer;
    U32 plain[kMaxAxes] = {0, 0, 0, 0};
    const std::size_t bag = writer.addBag();
    writer.addAsset(7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    std::vector<unsigned char> noAxes;
    assert(writer.build(kStamp, noAxes) == Writer::BUILD_OK);
    std::size_t payloadSize = 0;
    const std::size_t axesHeader =
        FindChunkHeader(noAxes, FourCC('A', 'X', 'E', 'S'), payloadSize);
    const std::size_t chunkSize =
        kChunkHeaderBytes + AlignUp(payloadSize, kPayloadAlign);
    noAxes.erase(noAxes.begin() + static_cast<std::vector<unsigned char>::difference_type>(axesHeader),
                 noAxes.begin() + static_cast<std::vector<unsigned char>::difference_type>(axesHeader + chunkSize));
    WriteU32BE(&noAxes[4],
               static_cast<U32>(noAxes.size() - kFormHeaderBytes));
    WriteU32BE(&noAxes[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(noAxes.size()));
    RestampHead(noAxes);
    assert(reader.openBorrowedBytes(&noAxes[0],
                                    noAxes.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }

  printf("==== [testLrpkChecksTheChunkThatDecidesSelection] end ====\n");
}
