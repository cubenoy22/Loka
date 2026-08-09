#include "LrpkFormatTests.hpp"
#include "support/TestVerify.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "LrpkTestByteSource.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/BlobRange.hpp"
#include "core/resource/lrpk/LrpkReader.hpp"
#include "lrpc/LrpkWriter.hpp"

using namespace loka::core::resource::lrpk;
using loka::core::resource::Blob;
using loka::core::resource::BlobRangeIsUsable;
using loka::lrpc::AssetLayoutKey;
using loka::lrpc::Writer;
using loka::lrpktests::MemoryByteSource;

namespace
{
  const U32 kStamp = 0xC0FFEE01UL;
  const std::size_t kAxisDepth = 0;
  const std::size_t kAxisAppearance = 0;
  const std::size_t kAxisScale = 1;

  const unsigned char kDefault[] = {'D', 'E', 'F', 'A', 'U', 'L', 'T'};
  const unsigned char kBw[] = {'B', 'W'};
  const unsigned char kFourBit[] = {'F', 'O', 'U', 'R', 'B', 'I', 'T'};
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
    writer.addAsset(AssetLayoutKey(""), 42, bag, ASSET_KIND_IMAGE, axes, kDefault, sizeof(kDefault));
    axes[kAxisDepth] = 1;
    writer.addAsset(AssetLayoutKey(""), 42, bag, ASSET_KIND_IMAGE, axes, kBw, sizeof(kBw));
    axes[kAxisDepth] = 0;
    axes[kAxisScale] = 1;
    writer.addAsset(AssetLayoutKey(""), 42, bag, ASSET_KIND_IMAGE, axes, k2x, sizeof(k2x));
    axes[kAxisScale] = 2;
    writer.addAsset(AssetLayoutKey(""), 42, bag, ASSET_KIND_IMAGE, axes, k3x, sizeof(k3x));
    U32 plain[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(AssetLayoutKey(""), 43, bag, ASSET_KIND_STRING, plain, kFile, sizeof(kFile));
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
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, axes, kDefault, sizeof(kDefault));
    axes[kAxisAppearance] = 1;
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, axes, kDark, sizeof(kDark));
    axes[kAxisAppearance] = 0;
    axes[kAxisScale] = 1;
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, axes, k2x, sizeof(k2x));
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
    writer.addAsset(AssetLayoutKey(""), 81, bag, ASSET_KIND_IMAGE, axes, kDefault, sizeof(kDefault));
    axes[0] = selectedValueFirst ? 1 : 2;
    writer.addAsset(AssetLayoutKey(""), 81, bag, ASSET_KIND_IMAGE, axes, kDark, sizeof(kDark));
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

  void AppendUnindexedDataWord(std::vector<unsigned char> &bytes)
  {
    std::size_t dataSize = 0;
    const std::size_t dataHeader =
        FindChunkHeader(bytes, FourCC('D', 'A', 'T', 'A'), dataSize);
    assert(dataHeader + kChunkHeaderBytes +
               AlignUp(dataSize, kPayloadAlign) ==
           bytes.size());
    bytes.push_back(0);
    bytes.push_back(0);
    bytes.push_back(0);
    bytes.push_back(0);
    WriteU32BE(&bytes[dataHeader + 4],
               static_cast<U32>(dataSize + kPayloadAlign));
    WriteU32BE(&bytes[4],
               static_cast<U32>(bytes.size() - kFormHeaderBytes));
    WriteU32BE(&bytes[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(bytes.size()));
    // Deliberately leave kHeadDataHeaderCrc stale. This is the multi-bit
    // corruption shape that remains structurally well formed.
    RestampHead(bytes);
  }

  void InsertEmptyBagRows(std::vector<unsigned char> &bytes,
                          std::size_t additionalBags)
  {
    std::size_t indexSize = 0;
    const std::size_t indexHeader =
        FindChunkHeader(bytes, FourCC('I', 'N', 'D', 'X'), indexSize);
    const std::size_t indexPayload = indexHeader + kChunkHeaderBytes;
    const std::size_t oldBagCount =
        static_cast<std::size_t>(ReadU32BE(&bytes[indexPayload]));
    const std::size_t rowsAt =
        indexPayload + 8 + oldBagCount * kBagRowBytes;
    std::vector<unsigned char> emptyRows(additionalBags * kBagRowBytes, 0);
    bytes.insert(bytes.begin() +
                     static_cast<std::vector<unsigned char>::difference_type>(rowsAt),
                 emptyRows.begin(),
                 emptyRows.end());
    const std::size_t newBagCount = oldBagCount + additionalBags;
    WriteU32BE(&bytes[indexHeader + 4],
               static_cast<U32>(indexSize + emptyRows.size()));
    WriteU32BE(&bytes[indexPayload], static_cast<U32>(newBagCount));
    WriteU32BE(&bytes[kHeadPayloadOffset + kHeadBagCount],
               static_cast<U32>(newBagCount));
    WriteU32BE(&bytes[4],
               static_cast<U32>(bytes.size() - kFormHeaderBytes));
    WriteU32BE(&bytes[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(bytes.size()));
    RestampChunk(bytes, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);
  }

  void ReorderChunks(std::vector<unsigned char> &bytes,
                     U32 firstTag,
                     U32 secondTag,
                     U32 thirdTag)
  {
    // A pure permutation: every chunk moves with its header and padding
    // intact, and no stored CRC covers a chunk's position, so nothing is
    // restamped. A refusal of the result therefore pins the order rule
    // itself, not a stale checksum.
    const U32 order[3] = {firstTag, secondTag, thirdTag};
    std::vector<unsigned char> stream;
    for (std::size_t i = 0; i < 3; ++i)
    {
      std::size_t payloadSize = 0;
      const std::size_t header = FindChunkHeader(bytes, order[i], payloadSize);
      assert(header < bytes.size());
      const std::size_t span =
          kChunkHeaderBytes + AlignUp(payloadSize, kPayloadAlign);
      stream.insert(stream.end(),
                    bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(header),
                    bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(header + span));
    }
    assert(stream.size() == bytes.size() - kFixedHeadBytes);
    std::copy(stream.begin(),
              stream.end(),
              bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(kFixedHeadBytes));
  }

  /** Runs the two-call file-backed open the way an application would, and
      reports whichever half refused. `indexBuffer` is the application's
      allocation and must outlive the reader whenever this answers OPEN_OK.

      A refusal from `beginOpen` ends it: calling `finishOpen` anyway would
      answer OPEN_NO_PENDING and mask the reason the open was refused. */
  Reader::OpenResult OpenThroughStream(Reader &reader,
                                       ByteSource &source,
                                       U32 expectedIdSpaceStamp,
                                       Reader::IntegrityMode integrityMode,
                                       std::vector<unsigned char> &indexBuffer)
  {
    std::size_t need = 0;
    const Reader::OpenResult begun =
        reader.beginOpen(source, expectedIdSpaceStamp, integrityMode, need);
    if (begun != Reader::OPEN_OK)
    {
      assert(need == 0 && "a refused beginOpen asks for nothing");
      return begun;
    }
    indexBuffer.assign(need, 0);
    return reader.finishOpen(need > 0 ? &indexBuffer[0] : 0, need);
  }

  /** The corpus gate: one package, four answers that must agree -- two
      integrity modes crossed with two transports.

      The transport half is what makes this worth doing. A refusal only the
      resident scanner knows how to produce, or one only the file-backed probe
      reaches, is a second grammar for one format; the cheapest way to find
      one is to make every malformed fixture in the suite answer twice. */
  void ExpectOpenResultInBothModes(const std::vector<unsigned char> &package,
                                   Reader::OpenResult expected)
  {
    Reader verified;
    LOKA_VERIFY(verified.openBorrowedBytes(&package[0],
                                      package.size(),
                                      kStamp,
                                      Reader::VERIFY_INTEGRITY) == expected);
    Reader unchecked;
    LOKA_VERIFY(unchecked.openBorrowedBytes(&package[0],
                                       package.size(),
                                       kStamp,
                                       Reader::SKIP_INTEGRITY) == expected);

    MemoryByteSource verifiedSource(package);
    std::vector<unsigned char> verifiedIndex;
    Reader verifiedStream;
    LOKA_VERIFY(OpenThroughStream(verifiedStream,
                             verifiedSource,
                             kStamp,
                             Reader::VERIFY_INTEGRITY,
                             verifiedIndex) == expected);
    MemoryByteSource uncheckedSource(package);
    std::vector<unsigned char> uncheckedIndex;
    Reader uncheckedStream;
    LOKA_VERIFY(OpenThroughStream(uncheckedStream,
                             uncheckedSource,
                             kStamp,
                             Reader::SKIP_INTEGRITY,
                             uncheckedIndex) == expected);
  }

  /** The file-backed answer for one package and one integrity mode, for the
      corpus entries whose two modes disagree and so cannot go through
      `ExpectOpenResultInBothModes`. Returning after the reader and its index
      buffer have both gone is safe precisely because a refusal commits
      nothing and an acceptance is not inspected here. */
  Reader::OpenResult StreamOpenResult(const std::vector<unsigned char> &package,
                                      Reader::IntegrityMode integrityMode)
  {
    MemoryByteSource source(package);
    std::vector<unsigned char> index;
    Reader reader;
    return OpenThroughStream(reader, source, kStamp, integrityMode, index);
  }

  /** Where `beginOpen`'s slice ends and the DATA payload begins, read out of
      the package bytes rather than out of the reader, so a test can arm a
      lying source before the reader has been asked anything. */
  void LocateDataChunk(const std::vector<unsigned char> &package,
                       std::size_t &headerAt,
                       std::size_t &payloadAt)
  {
    std::size_t payloadSize = 0;
    headerAt = FindChunkHeader(package, FourCC('D', 'A', 'T', 'A'), payloadSize);
    assert(headerAt < package.size());
    payloadAt = headerAt + kChunkHeaderBytes;
  }

  /** Cuts a package short and re-declares the two lengths that describe it,
      so the only thing wrong with the result is that bytes the chunk stream
      needs are missing. */
  void TruncateTo(std::vector<unsigned char> &bytes, std::size_t size)
  {
    assert(size >= kFixedHeadBytes && size <= bytes.size());
    bytes.resize(size);
    WriteU32BE(&bytes[4], static_cast<U32>(bytes.size() - kFormHeaderBytes));
    WriteU32BE(&bytes[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(bytes.size()));
    RestampHead(bytes);
  }

  /** A source that answers `beginOpen`'s probes from one package and
      `finishOpen`'s slice read from another -- the inconsistent liar the
      one-opaque-read interface cannot rule out. The slice read is the one
      read that starts at the end of the fixed head and is larger than a
      chunk header; everything else is a probe or the head. */
  class TwoFacedByteSource : public ByteSource
  {
  public:
    TwoFacedByteSource(const std::vector<unsigned char> &probeFace,
                       const std::vector<unsigned char> &sliceFace)
        : probeFace_(probeFace),
          sliceFace_(sliceFace)
    {
    }

    virtual bool readAt(std::size_t at, unsigned char *dst, std::size_t n)
    {
      const std::vector<unsigned char> &face =
          at == kFixedHeadBytes && n > kChunkHeaderBytes ? sliceFace_
                                                         : probeFace_;
      if (at > face.size() || face.size() - at < n)
      {
        return false;
      }
      if (n > 0)
      {
        std::memcpy(dst, &face[at], n);
      }
      return true;
    }

    virtual bool size(std::size_t &out)
    {
      out = probeFace_.size();
      return true;
    }

  private:
    TwoFacedByteSource(const TwoFacedByteSource &);
    TwoFacedByteSource &operator=(const TwoFacedByteSource &);

    const std::vector<unsigned char> &probeFace_;
    const std::vector<unsigned char> &sliceFace_;
  };

  /** Two non-empty bags with distinct ids, so both can be open at once and a
      test can leave one of them unread. */
  Writer::BuildResult BuildTwoBagPackage(std::vector<unsigned char> &out)
  {
    Writer writer;
    const std::size_t first = writer.addBag();
    const std::size_t second = writer.addBag();
    U32 plain[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(AssetLayoutKey(""), 11, first, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    writer.addAsset(AssetLayoutKey(""), 22, second, ASSET_KIND_STRING, plain, kFile, sizeof(kFile));
    return writer.build(kStamp, out);
  }

  /** A bag with a zero-length asset beside a normal one, and a second bag
      with nothing in it at all. Both legal shapes the golden fixture does not
      contain, and both the ones where a null base or a zero-length read is
      the difference between a pointer and a lie. */
  Writer::BuildResult BuildEmptyBagPackage(std::vector<unsigned char> &out)
  {
    Writer writer;
    const std::size_t full = writer.addBag();
    writer.addBag();
    U32 plain[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(AssetLayoutKey(""), 11, full, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    writer.addAsset(AssetLayoutKey(""), 12, full, ASSET_KIND_STRING, plain, 0, 0);
    return writer.build(kStamp, out);
  }

  /** Reads a bag the way the application is meant to: ask the size, provide
      exactly that, hand it over. */
  Reader::BagResult ReadBagIntoVector(Reader &reader,
                                      std::size_t bagIndex,
                                      std::vector<unsigned char> &buffer)
  {
    std::size_t stored = 0;
    LOKA_VERIFY(reader.bagStoredSize(bagIndex, stored));
    buffer.assign(stored, 0);
    if (stored == 0)
    {
      return reader.readBagInto(bagIndex, 0, 0);
    }
    // A vector's storage comes from operator new, which is aligned for every
    // fundamental type, so kPayloadAlign is satisfied without arranging it.
    assert(reinterpret_cast<std::size_t>(&buffer[0]) % kPayloadAlign == 0);
    return reader.readBagInto(bagIndex, &buffer[0], stored);
  }

  void OpenOneBag(Reader &reader,
                  const std::vector<unsigned char> &package)
  {
    LOKA_VERIFY(reader.openBorrowedBytes(&package[0],
                                    package.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
    LOKA_VERIFY(reader.openBag(0) == Reader::BAG_OK);
  }

  void ExpectSelection(const std::vector<unsigned char> &package,
                       const Facts &facts,
                       const unsigned char *bytes,
                       std::size_t length)
  {
    Reader reader;
    OpenOneBag(reader, package);
    Asset asset;
    LOKA_VERIFY(reader.get(42, facts, asset) == Reader::GET_OK);
    (void)bytes;
    (void)length;
    assert(AssetEquals(asset, bytes, length));
  }
} // namespace

void testLrpkRoundTripsThroughTheIndex()
{
  (void)&AssetEquals;
  printf("\n==== [testLrpkRoundTripsThroughTheIndex] start ====\n");

  std::vector<unsigned char> package;
  LOKA_VERIFY(BuildDepthScalePackage(package, true) == Writer::BUILD_OK);
  assert(package.size() % kPayloadAlign == 0);

  // Byte-level writer/reader cross-check for the ruled HEAD and AXES layout.
  const unsigned char *head = &package[kHeadPayloadOffset];
  assert(ReadU32BE(&package[0]) == FourCC('L', 'R', 'P', 'K'));
  assert(ReadU32BE(&package[kFormHeaderBytes]) == FourCC('H', 'E', 'A', 'D'));
  (void)head;
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
  (void)axesPayload;
  assert(package[axesPayload] == 2);
  assert(package[axesPayload + 4 + kAxisPrecedenceRank] == 0);
  assert(package[axesPayload + 4 + kAxisEntryBytes + kAxisPrecedenceRank] == 1);
  assert(Crc32::Of(&package[axesHeader], kChunkHeaderBytes + axesSize) ==
         ReadU32BE(head + kHeadAxesCrc));

  std::size_t indexSize = 0;
  const std::size_t indexHeader =
      FindChunkHeader(package, FourCC('I', 'N', 'D', 'X'), indexSize);
  (void)indexHeader;
  assert(Crc32::Of(&package[indexHeader], kChunkHeaderBytes + indexSize) ==
         ReadU32BE(head + kHeadIndexCrc));
  std::size_t dataSize = 0;
  const std::size_t dataHeader =
      FindChunkHeader(package, FourCC('D', 'A', 'T', 'A'), dataSize);
  (void)dataHeader;
  assert(Crc32::Of(&package[dataHeader], kChunkHeaderBytes) ==
         ReadU32BE(head + kHeadDataHeaderCrc));

  Reader reader;
  LOKA_VERIFY(reader.openBorrowedBytes(&package[0],
                                  package.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  assert(reader.verifiesIntegrity());
  assert(reader.bagCount() == 1);
  assert(reader.assetCount() == 5);

  Facts facts;
  Asset asset;
  LOKA_VERIFY(reader.get(42, facts, asset) == Reader::GET_BAG_NOT_OPEN);
  LOKA_VERIFY(reader.get(999, facts, asset) == Reader::GET_NO_SUCH_ID);
  LOKA_VERIFY(reader.openBag(0) == Reader::BAG_OK);
  LOKA_VERIFY(reader.get(42, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kDefault, sizeof(kDefault)));
  assert(asset.kind == ASSET_KIND_IMAGE);
  LOKA_VERIFY(reader.get(43, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kFile, sizeof(kFile)));
  assert(asset.bytes > &package[0] &&
         asset.bytes < &package[0] + package.size());
  reader.closeBag(0);
  LOKA_VERIFY(reader.get(42, facts, asset) == Reader::GET_BAG_NOT_OPEN);

  printf("==== [testLrpkRoundTripsThroughTheIndex] end ====\n");
}

void testLrpkSelectsByPackagePrecedence()
{
  printf("\n==== [testLrpkSelectsByPackagePrecedence] start ====\n");

  std::vector<unsigned char> depthFirst;
  std::vector<unsigned char> scaleFirst;
  LOKA_VERIFY(BuildDepthScalePackage(depthFirst, true) == Writer::BUILD_OK);
  LOKA_VERIFY(BuildDepthScalePackage(scaleFirst, false) == Writer::BUILD_OK);

  // Frozen acceptance truth table: depth=bw, scale=150.
  Facts bw150;
  bw150.present[kAxisDepth] = true;
  bw150.value[kAxisDepth] = 1;
  bw150.present[kAxisScale] = true;
  bw150.value[kAxisScale] = 150;
  ExpectSelection(depthFirst, bw150, kBw, sizeof(kBw));
  ExpectSelection(scaleFirst, bw150, k2x, sizeof(k2x));

  // The declared depth vocabulary is {1}; depth=4 is a genuine non-member
  // value and removes @bw during eligibility before precedence runs.
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
  // Presence is authoritative: stale storage must not resurrect a row that
  // writes the absent scalar axis.
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
  LOKA_VERIFY(BuildAppearanceScalePackage(appearanceFirst, true) == Writer::BUILD_OK);
  LOKA_VERIFY(BuildAppearanceScalePackage(appearanceScaleFirst, false) == Writer::BUILD_OK);
  Facts dark150;
  dark150.present[kAxisAppearance] = true;
  dark150.value[kAxisAppearance] = 1;
  dark150.present[kAxisScale] = true;
  dark150.value[kAxisScale] = 150;
  {
    Reader reader;
    OpenOneBag(reader, appearanceFirst);
    Asset asset;
    LOKA_VERIFY(reader.get(7, dark150, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDark, sizeof(kDark)));
  }
  {
    Reader reader;
    OpenOneBag(reader, appearanceScaleFirst);
    Asset asset;
    LOKA_VERIFY(reader.get(7, dark150, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, k2x, sizeof(k2x)));
  }

  // Enum facts carry declared vocabulary values, not their physical slots.
  // Reordering the vocabulary must not change the meaning of a caller fact.
  {
    std::vector<unsigned char> selectedValueFirst;
    std::vector<unsigned char> selectedValueSecond;
    LOKA_VERIFY(BuildEnumVocabularyPackage(selectedValueFirst, true) == Writer::BUILD_OK);
    LOKA_VERIFY(BuildEnumVocabularyPackage(selectedValueSecond, false) == Writer::BUILD_OK);
    Facts declaredTen;
    declaredTen.present[0] = true;
    declaredTen.value[0] = 10;
    Reader reader;
    OpenOneBag(reader, selectedValueFirst);
    Asset asset;
    LOKA_VERIFY(reader.get(81, declaredTen, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDark, sizeof(kDark)));
    OpenOneBag(reader, selectedValueSecond);
    LOKA_VERIFY(reader.get(81, declaredTen, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDark, sizeof(kDark)));
  }
  {
    // Two written slots in one package pin value-to-slot correspondence.
    Writer writer;
    const U32 depthValues[2] = {1, 4};
    writer.declareAxis(AXIS_KIND_ENUM, 0, depthValues, 2);
    const std::size_t bag = writer.addBag();
    U32 axes[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(AssetLayoutKey(""), 82, bag, ASSET_KIND_IMAGE, axes, kDefault, sizeof(kDefault));
    axes[kAxisDepth] = 1;
    writer.addAsset(AssetLayoutKey(""), 82, bag, ASSET_KIND_IMAGE, axes, kBw, sizeof(kBw));
    axes[kAxisDepth] = 2;
    writer.addAsset(AssetLayoutKey(""), 82, bag, ASSET_KIND_IMAGE, axes, kFourBit, sizeof(kFourBit));
    std::vector<unsigned char> package;
    LOKA_VERIFY(writer.build(kStamp, package) == Writer::BUILD_OK);

    Reader reader;
    OpenOneBag(reader, package);
    Asset asset;
    Facts depth1;
    depth1.present[kAxisDepth] = true;
    depth1.value[kAxisDepth] = 1;
    LOKA_VERIFY(reader.get(82, depth1, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kBw, sizeof(kBw)));
    Facts depth4;
    depth4.present[kAxisDepth] = true;
    depth4.value[kAxisDepth] = 4;
    LOKA_VERIFY(reader.get(82, depth4, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kFourBit, sizeof(kFourBit)));
    Facts depth2;
    depth2.present[kAxisDepth] = true;
    depth2.value[kAxisDepth] = 2;
    LOKA_VERIFY(reader.get(82, depth2, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDefault, sizeof(kDefault)));
    Facts noDepthFact;
    LOKA_VERIFY(reader.get(82, noDepthFact, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDefault, sizeof(kDefault)));
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
    LOKA_VERIFY(reader.get(42, noFacts, asset) == Reader::GET_NO_MATCHING_REP);
  }

  printf("==== [testLrpkSelectsByPackagePrecedence] end ====\n");
}

void testLrpkRefusesEveryCheckValueFailure()
{
  printf("\n==== [testLrpkRefusesEveryCheckValueFailure] start ====\n");

  std::vector<unsigned char> good;
  LOKA_VERIFY(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  Reader reader;
  LOKA_VERIFY(reader.openBorrowedBytes(&good[0],
                                  good.size(),
                                  kStamp + 1,
                                  Reader::VERIFY_INTEGRITY) ==
         Reader::OPEN_ID_SPACE_MISMATCH);

  {
    std::vector<unsigned char> shortened(good.begin(),
                                         good.end() - kPayloadAlign);
    LOKA_VERIFY(reader.openBorrowedBytes(&shortened[0],
                                    shortened.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_TRUNCATED);
    LOKA_VERIFY(StreamOpenResult(shortened, Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_TRUNCATED);
  }
  {
    std::vector<unsigned char> rotted(good);
    std::size_t indexSize = 0;
    const std::size_t indexAt =
        FindChunkPayload(rotted, FourCC('I', 'N', 'D', 'X'), indexSize);
    rotted[indexAt + indexSize - 1] ^= 0xFF;
    LOKA_VERIFY(reader.openBorrowedBytes(&rotted[0],
                                    rotted.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_INDEX_CORRUPT);
    LOKA_VERIFY(StreamOpenResult(rotted, Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_INDEX_CORRUPT);
  }
  {
    std::vector<unsigned char> rotted(good);
    std::size_t dataSize = 0;
    const std::size_t dataAt =
        FindChunkPayload(rotted, FourCC('D', 'A', 'T', 'A'), dataSize);
    rotted[dataAt] ^= 0xFF;
    LOKA_VERIFY(reader.openBorrowedBytes(&rotted[0],
                                    rotted.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    LOKA_VERIFY(reader.openBag(0) == Reader::BAG_CONTENTS_CORRUPT);
    // A rotted payload is past the file-backed slice, so the open agrees and
    // the disagreement waits for the bag read.
    LOKA_VERIFY(StreamOpenResult(rotted, Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
  }
  {
    std::vector<unsigned char> badHeader(good);
    AppendUnindexedDataWord(badHeader);
    LOKA_VERIFY(reader.openBorrowedBytes(&badHeader[0],
                                    badHeader.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_INDEX_CORRUPT);
    LOKA_VERIFY(reader.openBorrowedBytes(&badHeader[0],
                                    badHeader.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_OK);
    LOKA_VERIFY(StreamOpenResult(badHeader, Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_INDEX_CORRUPT);
    LOKA_VERIFY(StreamOpenResult(badHeader, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_OK);
  }
  {
    std::vector<unsigned char> unsupported(good);
    WriteU32BE(&unsupported[kHeadPayloadOffset + kHeadVersion],
               kFormatVersion + 1);
    RestampHead(unsupported);
    ExpectOpenResultInBothModes(unsupported,
                                Reader::OPEN_UNSUPPORTED_VERSION);
  }
  {
    std::vector<unsigned char> junk(kFixedHeadBytes, 0);
    LOKA_VERIFY(reader.openBorrowedBytes(&junk[0],
                                    junk.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_NOT_A_PACKAGE);
    LOKA_VERIFY(StreamOpenResult(junk, Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_NOT_A_PACKAGE);
  }

  printf("==== [testLrpkRefusesEveryCheckValueFailure] end ====\n");
}

void testLrpkOpenControlsIntegrityVerification()
{
  printf("\n==== [testLrpkOpenControlsIntegrityVerification] start ====\n");

  std::vector<unsigned char> package;
  LOKA_VERIFY(BuildDepthScalePackage(package, true) == Writer::BUILD_OK);
  std::vector<unsigned char> rotted(package);
  std::size_t dataSize = 0;
  const std::size_t dataAt =
      FindChunkPayload(rotted, FourCC('D', 'A', 'T', 'A'), dataSize);
  rotted[dataAt] ^= 0xFF;

  Reader unchecked;
  LOKA_VERIFY(unchecked.openBorrowedBytes(&rotted[0],
                                     rotted.size(),
                                     kStamp,
                                     Reader::SKIP_INTEGRITY) ==
         Reader::OPEN_OK);
  assert(!unchecked.verifiesIntegrity());
  LOKA_VERIFY(unchecked.openBag(0) == Reader::BAG_OK);

  Reader wrongBuild;
  LOKA_VERIFY(wrongBuild.openBorrowedBytes(&package[0],
                                      package.size(),
                                      kStamp + 1,
                                      Reader::SKIP_INTEGRITY) ==
         Reader::OPEN_ID_SPACE_MISMATCH);

  {
    std::vector<unsigned char> unsupportedCodec(package);
    std::size_t indexSize = 0;
    const std::size_t indexAt =
        FindChunkPayload(unsupportedCodec, FourCC('I', 'N', 'D', 'X'), indexSize);
    unsupportedCodec[indexAt + 8 + kBagCodec] =
        static_cast<unsigned char>(CODEC_RLE);
    RestampChunk(unsupportedCodec, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);
    Reader reader;
    LOKA_VERIFY(reader.openBorrowedBytes(&unsupportedCodec[0],
                                    unsupportedCodec.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    LOKA_VERIFY(reader.openBag(0) == Reader::BAG_UNSUPPORTED_CODEC);
  }

  Reader noBag;
  LOKA_VERIFY(noBag.openBag(0) == Reader::BAG_NO_SUCH_BAG);

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
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, specialized, kBw, sizeof(kBw));
    LOKA_VERIFY(writer.build(kStamp, out) ==
           Writer::BUILD_ASSET_WITHOUT_DEFAULT_ROW);
  }
  {
    Writer writer;
    DeclareDepthScale(writer, true);
    const std::size_t bag = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, k2x, sizeof(k2x));
    LOKA_VERIFY(writer.build(kStamp, out) ==
           Writer::BUILD_SELECTOR_AMBIGUOUS);
  }

  // Duplicate ids across bags are valid package data, but the bags are not
  // co-openable.
  {
    Writer writer;
    const std::size_t ja = writer.addBag();
    const std::size_t en = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 100, ja, ASSET_KIND_STRING, plain, kFile, sizeof(kFile));
    writer.addAsset(AssetLayoutKey(""), 100, en, ASSET_KIND_STRING, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_OK);
    Reader reader;
    LOKA_VERIFY(reader.openBorrowedBytes(&out[0],
                                    out.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    LOKA_VERIFY(reader.openBag(ja) == Reader::BAG_OK);
    LOKA_VERIFY(reader.openBag(en) == Reader::BAG_ASSET_ID_CONFLICT);
    assert(!reader.isBagOpen(en));
    Facts facts;
    Asset asset;
    LOKA_VERIFY(reader.get(100, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kFile, sizeof(kFile)));
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
    writer.addAsset(AssetLayoutKey(""), 100, ja, ASSET_KIND_STRING, plain, kFile, sizeof(kFile));
    writer.addAsset(AssetLayoutKey(""), 100, en, ASSET_KIND_STRING, specialized, kBw, sizeof(kBw));
    LOKA_VERIFY(writer.build(kStamp, out) ==
           Writer::BUILD_ASSET_WITHOUT_DEFAULT_ROW);
  }
  {
    Writer writer;
    DeclareDepthScale(writer, true);
    const std::size_t bag = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_STRING, specialized, kBw, sizeof(kBw));
    LOKA_VERIFY(writer.build(kStamp, out) ==
           Writer::BUILD_ASSET_KIND_MISMATCH);
  }
  {
    Writer writer;
    DeclareDepthScale(writer, true);
    writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, 4, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_BAD_BAG_REFERENCE);
  }
  {
    Writer writer;
    const U32 scale[1] = {100};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, scale, 1);
    const std::size_t bag = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, out) ==
           Writer::BUILD_BAD_AXIS_VOCABULARY);
  }

  // Two or more axes require an exact package policy permutation.
  {
    Writer missing;
    const U32 one[1] = {1};
    missing.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    missing.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    const std::size_t bag = missing.addBag();
    missing.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(missing.build(kStamp, out) == Writer::BUILD_BAD_PRECEDENCE);
  }
  {
    Writer duplicate;
    const U32 one[1] = {1};
    duplicate.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    duplicate.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    const std::size_t bad[2] = {0, 0};
    duplicate.setRepresentationPrecedence(bad, 2);
    const std::size_t bag = duplicate.addBag();
    duplicate.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(duplicate.build(kStamp, out) == Writer::BUILD_BAD_PRECEDENCE);
  }
  {
    Writer incomplete;
    const U32 one[1] = {1};
    incomplete.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    incomplete.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    const std::size_t onlyOne[1] = {0};
    incomplete.setRepresentationPrecedence(onlyOne, 1);
    const std::size_t bag = incomplete.addBag();
    incomplete.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(incomplete.build(kStamp, out) == Writer::BUILD_BAD_PRECEDENCE);
  }
  {
    Writer unknown;
    const U32 one[1] = {1};
    unknown.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    unknown.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    const std::size_t bad[2] = {0, 2};
    unknown.setRepresentationPrecedence(bad, 2);
    const std::size_t bag = unknown.addBag();
    unknown.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(unknown.build(kStamp, out) == Writer::BUILD_BAD_PRECEDENCE);
  }

  printf("==== [testLrpcRefusesRowsThatWouldNotBeReachable] end ====\n");
}

void testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds()
{
  printf("\n==== [testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds] start ====\n");

  std::vector<unsigned char> good;
  LOKA_VERIFY(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  std::size_t indexSize = 0;
  const std::size_t indexAt =
      FindChunkPayload(good, FourCC('I', 'N', 'D', 'X'), indexSize);
  const std::size_t bagRow = indexAt + 8;
  Reader reader;

  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[bagRow + kBagExpandedSize],
               ReadU32BE(&bad[bagRow + kBagExpandedSize]) + 4096);
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
    LOKA_VERIFY(StreamOpenResult(bad, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[bagRow + kBagDataOffset], 0xFFFFFF00UL);
    WriteU32BE(&bad[bagRow + kBagStoredSize], 0x00000200UL);
    WriteU32BE(&bad[bagRow + kBagExpandedSize], 0x00000200UL);
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
    LOKA_VERIFY(StreamOpenResult(bad, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }

  printf("==== [testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds] end ====\n");
}

void testLrpkRefusesForgedCountsAndUnsortedRows()
{
  printf("\n==== [testLrpkRefusesForgedCountsAndUnsortedRows] start ====\n");

  {
    const std::size_t maximum = ~static_cast<std::size_t>(0);
    (void)maximum;
    assert(!ExtentFits(100, maximum, 2));
    assert(!ProductFits(100, (maximum / 16) + 1, 16));

    // Pin the word-level rule on every host and the actual host-size gate
    // whenever size_t can represent the negative case.
    assert(U32WordsFit(0, kU32Mask));
    assert(!U32WordsFit(1, 0));
    assert(U32ValueFits(kU32Mask));
    assert(SizeFitsU32(65535));
    const bool hostU32Wider = sizeof(U32) > 4;
    if (hostU32Wider)
    {
      U32 tooLargeU32 = kU32Mask;
      ++tooLargeU32;
      assert(!U32ValueFits(tooLargeU32));
    }
    const bool hostSizeWider = sizeof(std::size_t) > 4;
    if (hostSizeWider)
    {
      std::size_t tooLarge = 1;
      for (std::size_t byte = 0; byte < 4; ++byte)
      {
        tooLarge <<= 8;
      }
      assert(!SizeFitsU32(tooLarge));
      // Keep the buffer valid so its alignment cannot mask the size failure.
      std::vector<unsigned char> borrowedBytes(kPayloadAlign, 0);
      assert(reinterpret_cast<std::size_t>(&borrowedBytes[0]) % kPayloadAlign == 0);
      Reader oversized;
      LOKA_VERIFY(oversized.openBorrowedBytes(&borrowedBytes[0],
                                              tooLarge,
                                              kStamp,
                                              Reader::VERIFY_INTEGRITY) ==
                  Reader::OPEN_SIZE_OUT_OF_RANGE);
    }
  }

  std::vector<unsigned char> good;
  LOKA_VERIFY(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  std::size_t indexSize = 0;
  const std::size_t indexAt =
      FindChunkPayload(good, FourCC('I', 'N', 'D', 'X'), indexSize);
  Reader reader;

  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(bad.size() + kPayloadAlign));
    RestampHead(bad);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_TRUNCATED);
  }
  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadBagCount], 2);
    RestampHead(bad);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadAssetCount],
               ReadU32BE(&bad[kHeadPayloadOffset + kHeadAssetCount]) + 1);
    RestampHead(bad);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    InsertEmptyBagRows(bad, kMaxBags);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }

  {
    std::vector<unsigned char> bad(good);
    WriteU32BE(&bad[indexAt + 4], 0x10000000UL);
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadAssetCount], 0x10000000UL);
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
    LOKA_VERIFY(StreamOpenResult(bad, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    const std::size_t rowsAt = indexAt + 8 + kBagRowBytes;
    WriteU32BE(&bad[rowsAt + 4 * kAssetRowBytes + kRowId], 1);
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
    LOKA_VERIFY(StreamOpenResult(bad, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    const std::size_t rowsAt = indexAt + 8 + kBagRowBytes;
    bad[rowsAt + kRowBag] = static_cast<unsigned char>(kMaxBags);
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX &&
           "a nonexistent bag is malformed data, not GET_BAG_NOT_OPEN");
    LOKA_VERIFY(StreamOpenResult(bad, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
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
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, outOfRange, kBw, sizeof(kBw));
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_BAD_AXIS_REFERENCE);
  }
  {
    Writer writer;
    const U32 one[1] = {1};
    AxisKind invalidKind = AXIS_KIND_ENUM;
    const int invalidKindBits = 255;
    assert(sizeof(invalidKind) == sizeof(invalidKindBits));
    std::memcpy(&invalidKind, &invalidKindBits, sizeof(invalidKind));
    writer.declareAxis(invalidKind, 0, one, 1);
    const std::size_t bag = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_BAD_AXIS_KIND);
  }
  {
    Writer writer;
    AssetKind invalidKind = ASSET_KIND_UNKNOWN;
    const int invalidKindBits = 255;
    assert(sizeof(invalidKind) == sizeof(invalidKindBits));
    std::memcpy(&invalidKind, &invalidKindBits, sizeof(invalidKind));
    const std::size_t bag = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, bag, invalidKind, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_BAD_ASSET_KIND);
  }
  {
    Writer writer;
    const U32 one[1] = {1};
    for (std::size_t i = 0; i < kMaxAxes + 1; ++i)
    {
      writer.declareAxis(AXIS_KIND_ENUM, 0, one, 1);
    }
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_TOO_MANY_AXES);
  }
  {
    Writer writer;
    U32 values[kMaxAxisValues + 1];
    for (std::size_t i = 0; i < kMaxAxisValues + 1; ++i)
    {
      values[i] = static_cast<U32>(i + 1);
    }
    writer.declareAxis(AXIS_KIND_ENUM, 0, values, kMaxAxisValues + 1);
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_TOO_MANY_AXIS_VALUES);
  }
  {
    Writer writer;
    const U32 huge[1] = {65536};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, huge, 1);
    const std::size_t bag = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, out) ==
           Writer::BUILD_AXIS_VALUE_OUT_OF_RANGE);
  }
  const bool hostU32Wider = sizeof(U32) > 4;
  if (hostU32Wider)
  {
    U32 tooLarge = kU32Mask;
    ++tooLarge;

    Writer badStamp;
    const std::size_t stampBag = badStamp.addBag();
    badStamp.addAsset(AssetLayoutKey(""), 7,
                      stampBag,
                      ASSET_KIND_IMAGE,
                      plain,
                      kDefault,
                      sizeof(kDefault));
    LOKA_VERIFY(badStamp.build(tooLarge, out) == Writer::BUILD_SIZE_OUT_OF_RANGE);

    Writer badId;
    const std::size_t idBag = badId.addBag();
    badId.addAsset(AssetLayoutKey(""), tooLarge,
                   idBag,
                   ASSET_KIND_IMAGE,
                   plain,
                   kDefault,
                   sizeof(kDefault));
    LOKA_VERIFY(badId.build(kStamp, out) == Writer::BUILD_SIZE_OUT_OF_RANGE);

    Writer badBaseline;
    const U32 scale[1] = {200};
    badBaseline.declareAxis(AXIS_KIND_SCALAR, tooLarge, scale, 1);
    const std::size_t baselineBag = badBaseline.addBag();
    badBaseline.addAsset(AssetLayoutKey(""), 7,
                         baselineBag,
                         ASSET_KIND_IMAGE,
                         plain,
                         kDefault,
                         sizeof(kDefault));
    LOKA_VERIFY(badBaseline.build(kStamp, out) == Writer::BUILD_SIZE_OUT_OF_RANGE);
  }
  // A trailing empty bag used to form &data[data.size()] while computing its
  // zero-length CRC. The package must build and the empty bag must verify.
  {
    Writer writer;
    const std::size_t full = writer.addBag();
    const std::size_t empty = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, full, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_OK);
    Reader reader;
    LOKA_VERIFY(reader.openBorrowedBytes(&out[0],
                                    out.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    LOKA_VERIFY(reader.openBag(empty) == Reader::BAG_OK);
  }

  // kMaxBags has one named home shared by storage and validation.
  {
    Writer writer;
    for (std::size_t i = 0; i < kMaxBags; ++i)
    {
      writer.addBag();
    }
    writer.addAsset(AssetLayoutKey(""), 7, 0, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_OK);
    writer.addBag();
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_TOO_MANY_BAGS);
  }

  {
    std::vector<unsigned char> reused;
    LOKA_VERIFY(BuildDepthScalePackage(reused, true) == Writer::BUILD_OK);
    const std::vector<unsigned char> before(reused);
    Writer writer;
    writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, 9, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    LOKA_VERIFY(writer.build(kStamp, reused) == Writer::BUILD_BAD_BAG_REFERENCE);
    assert(reused == before);
  }

  printf("==== [testLrpcValidatesBeforeItPacks] end ====\n");
}

void testLrpcRoundTripsExactlySizedSelectors()
{
  printf("\n==== [testLrpcRoundTripsExactlySizedSelectors] start ====\n");

  Writer writer;
  DeclareDepthScale(writer, true);
  const std::size_t bag = writer.addBag();
  writer.addAsset(AssetLayoutKey(""), 42,
                  bag,
                  ASSET_KIND_IMAGE,
                  0,
                  kDefault,
                  sizeof(kDefault));

  U32 *selector = new U32[2];
  selector[kAxisDepth] = 1;
  selector[kAxisScale] = 2;
  writer.addAsset(AssetLayoutKey(""), 42,
                  bag,
                  ASSET_KIND_IMAGE,
                  selector,
                  k3x,
                  sizeof(k3x));
  delete[] selector;

  std::vector<unsigned char> package;
  LOKA_VERIFY(writer.build(kStamp, package) == Writer::BUILD_OK);

  Reader reader;
  LOKA_VERIFY(reader.openBorrowedBytes(&package[0],
                                  package.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) ==
         Reader::OPEN_OK);
  LOKA_VERIFY(reader.openBag(bag) == Reader::BAG_OK);
  Facts facts;
  facts.present[kAxisDepth] = true;
  facts.value[kAxisDepth] = 1;
  facts.present[kAxisScale] = true;
  facts.value[kAxisScale] = 300;
  Asset asset;
  LOKA_VERIFY(reader.get(42, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, k3x, sizeof(k3x)));

  printf("==== [testLrpcRoundTripsExactlySizedSelectors] end ====\n");
}

void testLrpcRefusesAxisDeclarationAfterAsset()
{
  printf("\n==== [testLrpcRefusesAxisDeclarationAfterAsset] start ====\n");

  Writer writer;
  const std::size_t bag = writer.addBag();
  writer.addAsset(AssetLayoutKey(""), 7,
                  bag,
                  ASSET_KIND_IMAGE,
                  0,
                  kDefault,
                  sizeof(kDefault));
  const U32 depth[1] = {1};
  writer.declareAxis(AXIS_KIND_ENUM, 0, depth, 1);
  std::vector<unsigned char> out;
  LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_AXIS_AFTER_ASSET);

  printf("==== [testLrpcRefusesAxisDeclarationAfterAsset] end ====\n");
}

void testLrpkReaderKeepsItsPackageWhenAReloadIsRefused()
{
  printf("\n==== [testLrpkReaderKeepsItsPackageWhenAReloadIsRefused] start ====\n");

  std::vector<unsigned char> good;
  LOKA_VERIFY(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  Reader reader;
  OpenOneBag(reader, good);

  std::vector<unsigned char> junk(kFixedHeadBytes, 0);
  LOKA_VERIFY(reader.openBorrowedBytes(&junk[0],
                                  junk.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) ==
         Reader::OPEN_NOT_A_PACKAGE);
  assert(reader.isOpen());
  assert(reader.isBagOpen(0));
  Facts facts;
  Asset asset;
  LOKA_VERIFY(reader.get(42, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kDefault, sizeof(kDefault)));

  LOKA_VERIFY(reader.openBorrowedBytes(&good[0],
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
  LOKA_VERIFY(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  std::size_t axesSize = 0;
  const std::size_t axesAt =
      FindChunkPayload(good, FourCC('A', 'X', 'E', 'S'), axesSize);
  Reader reader;

  {
    std::vector<unsigned char> rotted(good);
    rotted[axesAt + 4 + kAxisBaseline] ^= 0xFF;
    LOKA_VERIFY(reader.openBorrowedBytes(&rotted[0],
                                    rotted.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_INDEX_CORRUPT);
    LOKA_VERIFY(StreamOpenResult(rotted, Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_INDEX_CORRUPT);
  }
  {
    std::vector<unsigned char> bad(good);
    bad[axesAt + 4 + kAxisKind] = 9;
    RestampChunk(bad, FourCC('A', 'X', 'E', 'S'), kHeadAxesCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    bad[axesAt + 4 + kAxisEntryBytes + kAxisPrecedenceRank] = 0;
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX &&
           "precedence is structural even when CRC verification is skipped");
    LOKA_VERIFY(StreamOpenResult(bad, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    bad[axesAt + 4 + kAxisPrecedenceRank] = 255;
    RestampChunk(bad, FourCC('A', 'X', 'E', 'S'), kHeadAxesCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    bad[axesAt + 4 + kAxisPrecedenceRank] = 2;
    RestampChunk(bad, FourCC('A', 'X', 'E', 'S'), kHeadAxesCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    const std::size_t scalar =
        axesAt + 4 + kAxisEntryBytes;
    WriteU16BE(&bad[scalar + kAxisValues], 300);
    WriteU16BE(&bad[scalar + kAxisValues + 2], 200);
    RestampChunk(bad, FourCC('A', 'X', 'E', 'S'), kHeadAxesCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    // Forge an unused scalar vocabulary entry to the baseline. AXES is
    // structurally malformed even when no row later exposes the collision.
    Writer writer;
    const U32 scale[1] = {200};
    writer.declareAxis(AXIS_KIND_SCALAR, 100, scale, 1);
    const std::size_t bag = writer.addBag();
    U32 plain[kMaxAxes] = {0, 0, 0, 0};
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    std::vector<unsigned char> bad;
    LOKA_VERIFY(writer.build(kStamp, bad) == Writer::BUILD_OK);
    std::size_t axesPayloadSize = 0;
    const std::size_t axesPayload =
        FindChunkPayload(bad, FourCC('A', 'X', 'E', 'S'), axesPayloadSize);
    WriteU16BE(&bad[axesPayload + 4 + kAxisValues], 100);
    RestampChunk(bad, FourCC('A', 'X', 'E', 'S'), kHeadAxesCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    const std::size_t depth = axesAt + 4;
    bad[depth + kAxisValueCount] = 2;
    WriteU16BE(&bad[depth + kAxisValues + 2], 1);
    RestampChunk(bad, FourCC('A', 'X', 'E', 'S'), kHeadAxesCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    const std::size_t scalar =
        axesAt + 4 + kAxisEntryBytes;
    WriteU16BE(&bad[scalar + kAxisValues], 100);
    RestampChunk(bad, FourCC('A', 'X', 'E', 'S'), kHeadAxesCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    std::size_t indexSize = 0;
    const std::size_t indexAt =
        FindChunkPayload(bad, FourCC('I', 'N', 'D', 'X'), indexSize);
    const std::size_t rowsAt = indexAt + 8 + kBagRowBytes;
    WriteU16BE(&bad[rowsAt + kRowAxes], 2);
    RestampChunk(bad, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    std::size_t indexSize = 0;
    const std::size_t indexAt =
        FindChunkPayload(bad, FourCC('I', 'N', 'D', 'X'), indexSize);
    const std::size_t rowsAt = indexAt + 8 + kBagRowBytes;
    bad[rowsAt + 4 * kAssetRowBytes + kRowKind] = 255;
    RestampChunk(bad, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    bad[kHeadPayloadOffset + kHeadFlags + 3] = 1;
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_HEAD_CORRUPT);
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
    LOKA_VERIFY(StreamOpenResult(bad, Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_HEAD_CORRUPT);
    LOKA_VERIFY(StreamOpenResult(bad, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }
  {
    std::vector<unsigned char> bad(good);
    std::size_t ignored = 0;
    const std::size_t axesHeader =
        FindChunkHeader(bad, FourCC('A', 'X', 'E', 'S'), ignored);
    bad[axesHeader] = 'a';
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
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
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
    std::vector<unsigned char> noAxes;
    LOKA_VERIFY(writer.build(kStamp, noAxes) == Writer::BUILD_OK);
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
    LOKA_VERIFY(reader.openBorrowedBytes(&noAxes[0],
                                    noAxes.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
    LOKA_VERIFY(StreamOpenResult(noAxes, Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_MALFORMED_INDEX);
  }

  printf("==== [testLrpkChecksTheChunkThatDecidesSelection] end ====\n");
}

void testLrpkRequiresCanonicalChunkOrder()
{
  printf("\n==== [testLrpkRequiresCanonicalChunkOrder] start ====\n");

  const U32 axes = FourCC('A', 'X', 'E', 'S');
  const U32 indx = FourCC('I', 'N', 'D', 'X');
  const U32 data = FourCC('D', 'A', 'T', 'A');

  std::vector<unsigned char> good;
  LOKA_VERIFY(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);

  // Control: reassembling in the canonical order reproduces the writer's
  // bytes exactly, so the refusals below can only come from the order rule.
  {
    std::vector<unsigned char> canonical(good);
    ReorderChunks(canonical, axes, indx, data);
    assert(canonical == good &&
           "the writer already emits the canonical order");
    Reader reader;
    LOKA_VERIFY(reader.openBorrowedBytes(&canonical[0],
                                    canonical.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
  }

  // Every other permutation of the three required chunks is refused, in
  // both integrity modes.
  const U32 permutations[5][3] = {
      {indx, axes, data},
      {axes, data, indx},
      {indx, data, axes},
      {data, axes, indx},
      {data, indx, axes},
  };
  for (std::size_t p = 0; p < 5; ++p)
  {
    std::vector<unsigned char> permuted(good);
    ReorderChunks(permuted,
                  permutations[p][0],
                  permutations[p][1],
                  permutations[p][2]);
    ExpectOpenResultInBothModes(permuted, Reader::OPEN_MALFORMED_INDEX);
  }

  printf("==== [testLrpkRequiresCanonicalChunkOrder] end ====\n");
}

void testLrpkEnforcesPayloadAlignment()
{
  printf("\n==== [testLrpkEnforcesPayloadAlignment] start ====\n");

  std::vector<unsigned char> good;
  LOKA_VERIFY(BuildDepthScalePackage(good, true) == Writer::BUILD_OK);
  assert(reinterpret_cast<std::size_t>(&good[0]) % kPayloadAlign == 0);

  {
    std::vector<unsigned char> shifted(good.size() + 1);
    assert(reinterpret_cast<std::size_t>(&shifted[0]) % kPayloadAlign == 0);
    std::copy(good.begin(), good.end(), shifted.begin() + 1);
    Reader verified;
    LOKA_VERIFY(verified.openBorrowedBytes(&shifted[1],
                                      good.size(),
                                      kStamp,
                                      Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_MISALIGNED_BUFFER);
    Reader unchecked;
    LOKA_VERIFY(unchecked.openBorrowedBytes(&shifted[1],
                                       good.size(),
                                       kStamp,
                                       Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_MISALIGNED_BUFFER);
  }

  Writer writer;
  const std::size_t bag = writer.addBag();
  U32 plain[kMaxAxes] = {0, 0, 0, 0};
  writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, kDefault, sizeof(kDefault));
  std::vector<unsigned char> single;
  LOKA_VERIFY(writer.build(kStamp, single) == Writer::BUILD_OK);
  std::size_t indexSize = 0;
  const std::size_t indexAt =
      FindChunkPayload(single, FourCC('I', 'N', 'D', 'X'), indexSize);

  {
    std::vector<unsigned char> oddBag(single);
    const std::size_t stored =
        static_cast<std::size_t>(ReadU32BE(&oddBag[indexAt + 8 + kBagStoredSize]));
    assert(stored > 0);
    std::size_t dataSize = 0;
    const std::size_t dataAt =
        FindChunkPayload(oddBag, FourCC('D', 'A', 'T', 'A'), dataSize);
    assert(stored == dataSize);
    WriteU32BE(&oddBag[indexAt + 8 + kBagDataOffset], 1);
    WriteU32BE(&oddBag[indexAt + 8 + kBagStoredSize],
               static_cast<U32>(stored - 1));
    WriteU32BE(&oddBag[indexAt + 8 + kBagExpandedSize],
               static_cast<U32>(stored - 1));
    WriteU32BE(&oddBag[indexAt + 8 + kBagCrc],
               Crc32::Of(&oddBag[dataAt + 1], stored - 1));
    RestampChunk(oddBag, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);
    ExpectOpenResultInBothModes(oddBag, Reader::OPEN_MALFORMED_INDEX);
  }

  {
    std::vector<unsigned char> oddRow(single);
    const std::size_t rowsAt = indexAt + 8 + kBagRowBytes;
    WriteU32BE(&oddRow[rowsAt + kRowOffset], 1);
    RestampChunk(oddRow, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);
    ExpectOpenResultInBothModes(oddRow, Reader::OPEN_MALFORMED_INDEX);
  }

  Reader reader;
  LOKA_VERIFY(reader.openBorrowedBytes(&good[0],
                                  good.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  LOKA_VERIFY(reader.openBag(0) == Reader::BAG_OK);
  Facts cases[4];
  cases[1].present[kAxisDepth] = true;
  cases[1].value[kAxisDepth] = 1;
  cases[2].present[kAxisScale] = true;
  cases[2].value[kAxisScale] = 150;
  cases[3].present[kAxisScale] = true;
  cases[3].value[kAxisScale] = 250;
  const unsigned char *expected[4] = {kDefault, kBw, k2x, k3x};
  const std::size_t expectedLength[4] = {
      sizeof(kDefault), sizeof(kBw), sizeof(k2x), sizeof(k3x)};
  for (std::size_t i = 0; i < 4; ++i)
  {
    Asset asset;
    LOKA_VERIFY(reader.get(42, cases[i], asset) == Reader::GET_OK);
    (void)expected;
    (void)expectedLength;
    assert(AssetEquals(asset, expected[i], expectedLength[i]));
    assert(reinterpret_cast<std::size_t>(asset.bytes) % kPayloadAlign == 0);
  }
  Asset file;
  LOKA_VERIFY(reader.get(43, cases[0], file) == Reader::GET_OK);
  assert(AssetEquals(file, kFile, sizeof(kFile)));
  assert(reinterpret_cast<std::size_t>(file.bytes) % kPayloadAlign == 0);

  printf("==== [testLrpkEnforcesPayloadAlignment] end ====\n");
}

void testLrpcPreservesNullPayloadFailure()
{
  printf("\n==== [testLrpcPreservesNullPayloadFailure] start ====\n");

  U32 plain[kMaxAxes] = {0, 0, 0, 0};
  {
    Writer writer;
    const std::size_t bag = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, 0, 1);
    std::vector<unsigned char> out;
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_NULL_PAYLOAD);
  }
  {
    Writer writer;
    const std::size_t bag = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 7, bag, ASSET_KIND_IMAGE, plain, 0, 0);
    std::vector<unsigned char> out;
    LOKA_VERIFY(writer.build(kStamp, out) == Writer::BUILD_OK);
  }

  printf("==== [testLrpcPreservesNullPayloadFailure] end ====\n");
}

void testLrpcCanonicalBuildBytesStayStable()
{
  printf("\n==== [testLrpcCanonicalBuildBytesStayStable] start ====\n");

  Writer writer;
  const U32 appearances[2] = {10, 20};
  const U32 scales[2] = {200, 300};
  writer.declareAxis(AXIS_KIND_ENUM, 0, appearances, 2);
  writer.declareAxis(AXIS_KIND_SCALAR, 100, scales, 2);
  const std::size_t precedence[2] = {1, 0};
  writer.setRepresentationPrecedence(precedence, 2);
  const std::size_t bag0 = writer.addBag();
  const std::size_t bag1 = writer.addBag();

  const U32 plain[kMaxAxes] = {0, 0, 0, 0};
  const U32 appearance2[kMaxAxes] = {2, 0, 0, 0};
  const U32 appearance1Scale2[kMaxAxes] = {1, 2, 0, 0};
  const U32 scale1[kMaxAxes] = {0, 1, 0, 0};
  const U32 scale2[kMaxAxes] = {0, 2, 0, 0};
  const unsigned char id200Bag1Default[] = "id200-b1-default";
  const unsigned char id100Bag0Appearance2[] = "id100-b0-appearance2";
  const unsigned char id100Bag1Scale2[] = "id100-b1-scale2";
  const unsigned char id100Bag0Default[] = "id100-b0-default";
  const unsigned char id300Bag0Default[] = "id300-b0-default";
  const unsigned char id100Bag1Default[] = "id100-b1-default";
  const unsigned char id100Bag0Appearance1Scale2[] =
      "id100-b0-appearance1-scale2";
  const unsigned char id200Bag1Scale1[] = "id200-b1-scale1";

  // Scrambled insertion plus the same id in both bags pins the specified
  // id/axes/bag order, DATA layout, every embedded CRC, and final padding.
  writer.addAsset(AssetLayoutKey(""), 200, bag1, ASSET_KIND_IMAGE, plain,
                  id200Bag1Default, sizeof(id200Bag1Default) - 1);
  writer.addAsset(AssetLayoutKey(""), 100, bag0, ASSET_KIND_IMAGE, appearance2,
                  id100Bag0Appearance2, sizeof(id100Bag0Appearance2) - 1);
  writer.addAsset(AssetLayoutKey(""), 100, bag1, ASSET_KIND_IMAGE, scale2,
                  id100Bag1Scale2, sizeof(id100Bag1Scale2) - 1);
  writer.addAsset(AssetLayoutKey(""), 100, bag0, ASSET_KIND_IMAGE, plain,
                  id100Bag0Default, sizeof(id100Bag0Default) - 1);
  writer.addAsset(AssetLayoutKey(""), 300, bag0, ASSET_KIND_IMAGE, plain,
                  id300Bag0Default, sizeof(id300Bag0Default) - 1);
  writer.addAsset(AssetLayoutKey(""), 100, bag1, ASSET_KIND_IMAGE, plain,
                  id100Bag1Default, sizeof(id100Bag1Default) - 1);
  writer.addAsset(AssetLayoutKey(""), 100, bag0, ASSET_KIND_IMAGE, appearance1Scale2,
                  id100Bag0Appearance1Scale2,
                  sizeof(id100Bag0Appearance1Scale2) - 1);
  writer.addAsset(AssetLayoutKey(""), 200, bag1, ASSET_KIND_IMAGE, scale1,
                  id200Bag1Scale1, sizeof(id200Bag1Scale1) - 1);

  std::vector<unsigned char> package;
  LOKA_VERIFY(writer.build(kStamp, package) == Writer::BUILD_OK);
  assert(package.size() == 940);
  assert(Crc32::Of(&package[0], package.size()) == 0xB0EE89E7UL);

  printf("==== [testLrpcCanonicalBuildBytesStayStable] end ====\n");
}

void testLrpcBuildHandlesFiftyThousandAssets()
{
  printf("\n==== [testLrpcBuildHandlesFiftyThousandAssets] start ====\n");

  Writer writer;
  const std::size_t bag = writer.addBag();
  const U32 plain[kMaxAxes] = {0, 0, 0, 0};
  const unsigned char payload = 0x5A;
  const std::size_t assetCount = 50000;
  for (std::size_t i = 0; i < assetCount; ++i)
  {
    writer.addAsset(AssetLayoutKey(""), static_cast<U32>(assetCount - i),
                    bag,
                    ASSET_KIND_IMAGE,
                    plain,
                    &payload,
                    1);
  }

  std::vector<unsigned char> package;
  LOKA_VERIFY(writer.build(kStamp, package) == Writer::BUILD_OK);
  assert(ReadU32BE(&package[kHeadPayloadOffset + kHeadAssetCount]) ==
         static_cast<U32>(assetCount));

  printf("==== [testLrpcBuildHandlesFiftyThousandAssets] end ====\n");
}

void testLrpkStreamOpenMatchesMemoryOpen()
{
  printf("\n==== [testLrpkStreamOpenMatchesMemoryOpen] start ====\n");

  std::vector<unsigned char> package;
  LOKA_VERIFY(BuildTwoBagPackage(package) == Writer::BUILD_OK);

  Reader memory;
  LOKA_VERIFY(memory.openBorrowedBytes(&package[0],
                                  package.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  LOKA_VERIFY(memory.openBag(0) == Reader::BAG_OK);
  LOKA_VERIFY(memory.openBag(1) == Reader::BAG_OK);

  // Declared before the reader that borrows them: the source and the index
  // buffer must outlive it, and a test that gets that backwards is not
  // demonstrating the contract it exists to show.
  MemoryByteSource source(package);
  std::vector<unsigned char> index;
  std::vector<unsigned char> firstBag;
  std::vector<unsigned char> secondBag;
  Reader stream;
  std::size_t need = 0;
  LOKA_VERIFY(stream.beginOpen(source,
                          kStamp,
                          Reader::VERIFY_INTEGRITY,
                          need) == Reader::OPEN_OK);
  // The measured slice is exactly the file's [512, DATA payload start): chunk
  // headers included, DATA's header included, DATA's payload excluded. This
  // is the arithmetic the canonical chunk order was ruled for.
  std::size_t dataHeaderAt = 0;
  std::size_t dataPayloadAt = 0;
  LocateDataChunk(package, dataHeaderAt, dataPayloadAt);
  assert(need == dataPayloadAt - kFixedHeadBytes);

  // Nothing is observable between the two calls: an unopened reader still
  // answers as an unopened reader.
  assert(!stream.isOpen());
  assert(stream.bagCount() == 0);

  index.assign(need, 0);
  LOKA_VERIFY(stream.finishOpen(&index[0], need) == Reader::OPEN_OK);
  assert(stream.isOpen());
  assert(stream.verifiesIntegrity());
  assert(stream.bagCount() == memory.bagCount());
  assert(stream.assetCount() == memory.assetCount());
  assert(stream.idSpaceStamp() == memory.idSpaceStamp());

  std::size_t storedFirst = 0;
  std::size_t storedSecond = 0;
  LOKA_VERIFY(memory.bagStoredSize(0, storedFirst));
  LOKA_VERIFY(memory.bagStoredSize(1, storedSecond));
  LOKA_VERIFY(ReadBagIntoVector(stream, 0, firstBag) == Reader::BAG_OK);
  LOKA_VERIFY(ReadBagIntoVector(stream, 1, secondBag) == Reader::BAG_OK);
  assert(firstBag.size() == storedFirst && secondBag.size() == storedSecond);

  // Same assets, same truth (bag, offsetInBag, length), same bytes -- from
  // two different addresses, which is what makes the byte comparison mean
  // something.
  const U32 ids[2] = {11, 22};
  Facts facts;
  for (std::size_t i = 0; i < 2; ++i)
  {
    Asset fromMemory;
    Asset fromStream;
    LOKA_VERIFY(memory.get(ids[i], facts, fromMemory) == Reader::GET_OK);
    assert(stream.get(ids[i], facts, fromStream) == Reader::GET_OK);
    assert(fromStream.kind == fromMemory.kind);
    assert(fromStream.length == fromMemory.length);
    assert(fromStream.bag == fromMemory.bag);
    assert(fromStream.offsetInBag == fromMemory.offsetInBag);
    assert(fromStream.bytes != fromMemory.bytes &&
           "the file-backed reader serves the application's buffer");
    assert(std::memcmp(fromStream.bytes,
                       fromMemory.bytes,
                       fromMemory.length) == 0);
  }

  // The probe's mirror of the scanner. A tag V1 does not know, in the place
  // AXES must occupy, is an unknown chunk to both transports; a tag it does
  // know, out of turn, is an order violation to both. The existing corpus
  // pins the permutations; these two pin the shapes the probe alone sees
  // first.
  {
    std::vector<unsigned char> bad(package);
    std::size_t ignored = 0;
    const std::size_t axesHeader =
        FindChunkHeader(bad, FourCC('A', 'X', 'E', 'S'), ignored);
    bad[axesHeader] = 'a';
    Reader reader;
    LOKA_VERIFY(reader.openBorrowedBytes(&bad[0],
                                    bad.size(),
                                    kStamp,
                                    Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_UNKNOWN_CHUNK);
    LOKA_VERIFY(StreamOpenResult(bad, Reader::SKIP_INTEGRITY) ==
           Reader::OPEN_UNKNOWN_CHUNK);
  }
  {
    // The head fixes the row geometry before the application allocates the
    // stream index. Give INDX one aligned word beyond those rows while
    // keeping the package's chunk stream structurally complete.
    std::vector<unsigned char> bad(package);
    std::size_t indexSize = 0;
    const std::size_t indexHeader =
        FindChunkHeader(bad, FourCC('I', 'N', 'D', 'X'), indexSize);
    std::size_t dataSize = 0;
    const std::size_t oldDataHeader =
        FindChunkHeader(bad, FourCC('D', 'A', 'T', 'A'), dataSize);
    bad.insert(bad.begin() +
                   static_cast<std::vector<unsigned char>::difference_type>(
                       oldDataHeader),
               kPayloadAlign,
               0);
    WriteU32BE(&bad[indexHeader + 4],
               static_cast<U32>(indexSize + kPayloadAlign));
    WriteU32BE(&bad[4],
               static_cast<U32>(bad.size() - kFormHeaderBytes));
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(bad.size()));
    RestampChunk(bad, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);

    MemoryByteSource badSource(bad);
    Reader reader;
    std::size_t badNeed = 1;
    LOKA_VERIFY(reader.beginOpen(badSource,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            badNeed) == Reader::OPEN_MALFORMED_INDEX);
    assert(badNeed == 0 &&
           "contradicted row geometry cannot extract an allocation");
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    // Five payload bytes cannot encode any whole AXES vocabulary. Preserve
    // the following chunk boundary so both parsers reach that same fact.
    std::vector<unsigned char> bad(package);
    std::size_t axesSize = 0;
    const std::size_t axesHeader =
        FindChunkHeader(bad, FourCC('A', 'X', 'E', 'S'), axesSize);
    const std::size_t oldAxesSpan =
        kChunkHeaderBytes + AlignUp(axesSize, kPayloadAlign);
    bad.insert(bad.begin() +
                   static_cast<std::vector<unsigned char>::difference_type>(
                       axesHeader + oldAxesSpan),
               kPayloadAlign,
               0);
    WriteU32BE(&bad[axesHeader + 4], 5);
    WriteU32BE(&bad[4],
               static_cast<U32>(bad.size() - kFormHeaderBytes));
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(bad.size()));
    RestampChunk(bad, FourCC('A', 'X', 'E', 'S'), kHeadAxesCrc);

    MemoryByteSource badSource(bad);
    Reader reader;
    std::size_t badNeed = 1;
    LOKA_VERIFY(reader.beginOpen(badSource,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            badNeed) == Reader::OPEN_MALFORMED_INDEX);
    assert(badNeed == 0);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    // This is the original allocation-amplification shape: a normal package
    // is stretched until INDX claims nearly the whole file, while its forged
    // head declares that there are no rows at all.
    std::vector<unsigned char> bad(package);
    std::size_t indexSize = 0;
    const std::size_t indexHeader =
        FindChunkHeader(bad, FourCC('I', 'N', 'D', 'X'), indexSize);
    std::size_t dataSize = 0;
    const std::size_t oldDataHeader =
        FindChunkHeader(bad, FourCC('D', 'A', 'T', 'A'), dataSize);
    const std::size_t inflatedBytes = 1024 * 1024;
    bad.insert(bad.begin() +
                   static_cast<std::vector<unsigned char>::difference_type>(
                       oldDataHeader),
               inflatedBytes,
               0);
    WriteU32BE(&bad[indexHeader + 4],
               static_cast<U32>(indexSize + inflatedBytes));
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadAssetCount], 0);
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadBagCount], 0);
    WriteU32BE(&bad[4],
               static_cast<U32>(bad.size() - kFormHeaderBytes));
    WriteU32BE(&bad[kHeadPayloadOffset + kHeadTotalBytes],
               static_cast<U32>(bad.size()));
    RestampChunk(bad, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);

    MemoryByteSource badSource(bad);
    Reader reader;
    std::size_t badNeed = 1;
    LOKA_VERIFY(reader.beginOpen(badSource,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            badNeed) == Reader::OPEN_MALFORMED_INDEX);
    assert(badNeed == 0 &&
           "a zero-row head cannot extract the file-sized INDX allocation");
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    // A head and no chunk stream at all. The scanner's loop never runs and
    // finds AXES missing; the probe is asked to read at the end of a
    // zero-length stream. Both call that a malformed index rather than
    // truncation, which is reserved for a chunk that starts and does not
    // finish -- the next two cases.
    std::vector<unsigned char> bad(package);
    TruncateTo(bad, kFixedHeadBytes);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_MALFORMED_INDEX);
  }
  {
    // Half of the AXES header.
    std::vector<unsigned char> bad(package);
    TruncateTo(bad, kFixedHeadBytes + 4);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_TRUNCATED);
  }
  {
    // Half of the DATA header: the slice the file-backed open measures is
    // short of its last chunk header, and the resident scan runs out at the
    // same byte.
    std::vector<unsigned char> bad(package);
    TruncateTo(bad, dataHeaderAt + 4);
    ExpectOpenResultInBothModes(bad, Reader::OPEN_TRUNCATED);
  }
  {
    // The mirror of the memory path's own range gate: a source claiming a
    // total the format's 32-bit length field cannot hold is refused before
    // any of its bytes are believed. Only reachable where std::size_t is
    // wider than the field.
    const bool hostSizeWider = sizeof(std::size_t) > 4;
    if (hostSizeWider)
    {
      std::size_t tooLarge = 1;
      for (std::size_t byte = 0; byte < 4; ++byte)
      {
        tooLarge <<= 8;
      }
      MemoryByteSource lying(package);
      lying.reportSize(tooLarge);
      Reader reader;
      std::size_t lyingNeed = 0;
      LOKA_VERIFY(reader.beginOpen(lying,
                              kStamp,
                              Reader::VERIFY_INTEGRITY,
                              lyingNeed) == Reader::OPEN_SIZE_OUT_OF_RANGE);
      assert(lyingNeed == 0);
    }
  }

  printf("==== [testLrpkStreamOpenMatchesMemoryOpen] end ====\n");
}

void testLrpkStreamOpenIsFailureAtomic()
{
  printf("\n==== [testLrpkStreamOpenIsFailureAtomic] start ====\n");

  std::vector<unsigned char> package;
  LOKA_VERIFY(BuildTwoBagPackage(package) == Writer::BUILD_OK);
  std::size_t dataHeaderAt = 0;
  std::size_t dataPayloadAt = 0;
  LocateDataChunk(package, dataHeaderAt, dataPayloadAt);

  MemoryByteSource committedSource(package);
  std::vector<unsigned char> committedIndex;
  std::vector<unsigned char> firstBag;
  std::vector<unsigned char> secondBag;
  Reader reader;
  LOKA_VERIFY(OpenThroughStream(reader,
                           committedSource,
                           kStamp,
                           Reader::VERIFY_INTEGRITY,
                           committedIndex) == Reader::OPEN_OK);
  // Bag 1 is deliberately left unread: it is the thing a source swap would
  // destroy, and destroying it is the v2 hole this whole test exists for.
  LOKA_VERIFY(ReadBagIntoVector(reader, 0, firstBag) == Reader::BAG_OK);

  Facts facts;
  Asset asset;
  LOKA_VERIFY(reader.get(11, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kDefault, sizeof(kDefault)));

  // A reload refused in the first half.
  {
    std::vector<unsigned char> corrupt(package);
    corrupt[kHeadPayloadOffset + kHeadVersion + 3] ^= 0xFF;
    MemoryByteSource badSource(corrupt);
    std::size_t need = 0;
    LOKA_VERIFY(reader.beginOpen(badSource,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            need) == Reader::OPEN_HEAD_CORRUPT);
    assert(need == 0);
    assert(reader.isOpen() && reader.bagCount() == 2 && reader.isBagOpen(0));
    LOKA_VERIFY(reader.get(11, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDefault, sizeof(kDefault)));
  }

  // A reload refused in the second half, by a source that delivers the head
  // and both chunk headers and then fails on the slice. The window covers
  // the DATA chunk header, which beginOpen never reads and finishOpen always
  // does, so the failure lands in the second call without depending on how
  // many reads the first one made.
  {
    MemoryByteSource halfLyingSource(package);
    halfLyingSource.failReadsOver(dataHeaderAt, dataHeaderAt + kChunkHeaderBytes);
    std::size_t need = 0;
    LOKA_VERIFY(reader.beginOpen(halfLyingSource,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            need) == Reader::OPEN_OK);
    // Mid-window: the committed package is still the only one observable.
    assert(reader.isOpen() && reader.bagCount() == 2 && reader.isBagOpen(0));
    LOKA_VERIFY(reader.get(11, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDefault, sizeof(kDefault)));

    std::vector<unsigned char> index(need, 0);
    LOKA_VERIFY(reader.finishOpen(&index[0], need) == Reader::OPEN_SOURCE_FAILED);
    assert(reader.isOpen() && reader.bagCount() == 2 && reader.isBagOpen(0));
    LOKA_VERIFY(reader.get(11, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDefault, sizeof(kDefault)));

    // One shot: the pending was consumed by the refusal, so the retry is not
    // a second finishOpen.
    LOKA_VERIFY(reader.finishOpen(&index[0], need) == Reader::OPEN_NO_PENDING);
  }

  // The buffer is not a hint. A size the reader did not ask for is refused
  // before anything is read, and the committed package is untouched.
  {
    MemoryByteSource source(package);
    std::size_t need = 0;
    LOKA_VERIFY(reader.beginOpen(source,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            need) == Reader::OPEN_OK);
    std::vector<unsigned char> tooBig(need + 1, 0);
    LOKA_VERIFY(reader.finishOpen(&tooBig[0], need + 1) ==
           Reader::OPEN_INDEX_BUFFER_SIZE_MISMATCH);
    assert(reader.isOpen() && reader.isBagOpen(0));

    LOKA_VERIFY(reader.beginOpen(source,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            need) == Reader::OPEN_OK);
    std::vector<unsigned char> tooSmall(need - 1, 0);
    LOKA_VERIFY(reader.finishOpen(&tooSmall[0], need - 1) ==
           Reader::OPEN_INDEX_BUFFER_SIZE_MISMATCH);
    assert(reader.isOpen() && reader.isBagOpen(0));
  }

  // A finishOpen with nothing to finish, and a second one after a consumed
  // pending, are the same answer.
  {
    Reader fresh;
    std::vector<unsigned char> nothing(8, 0);
    LOKA_VERIFY(fresh.finishOpen(&nothing[0], nothing.size()) ==
           Reader::OPEN_NO_PENDING);
    assert(!fresh.isOpen());
  }

  // The point of all of it: the surviving package can still reach the bytes
  // it never loaded. A reader that had handed its source to a refused reload
  // would fail here.
  LOKA_VERIFY(ReadBagIntoVector(reader, 1, secondBag) == Reader::BAG_OK);
  LOKA_VERIFY(reader.get(22, facts, asset) == Reader::GET_OK);
  assert(AssetEquals(asset, kFile, sizeof(kFile)));

  printf("==== [testLrpkStreamOpenIsFailureAtomic] end ====\n");
}

void testLrpkStreamRefusesSourceLies()
{
  printf("\n==== [testLrpkStreamRefusesSourceLies] start ====\n");

  std::vector<unsigned char> package;
  LOKA_VERIFY(BuildTwoBagPackage(package) == Writer::BUILD_OK);
  std::size_t dataHeaderAt = 0;
  std::size_t dataPayloadAt = 0;
  LocateDataChunk(package, dataHeaderAt, dataPayloadAt);

  // A source that cannot say how big it is -- the 32-bit host with a file it
  // cannot describe, and the disconnected volume, arriving as the same
  // typed answer.
  {
    MemoryByteSource source(package);
    source.failSize();
    Reader reader;
    std::size_t need = 0;
    LOKA_VERIFY(reader.beginOpen(source,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            need) == Reader::OPEN_SOURCE_FAILED);
    assert(need == 0);
  }
  // A source that cannot deliver the fixed head.
  {
    MemoryByteSource source(package);
    source.failReadsOver(0, kFixedHeadBytes);
    Reader reader;
    std::size_t need = 0;
    LOKA_VERIFY(reader.beginOpen(source,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            need) == Reader::OPEN_SOURCE_FAILED);
  }
  // A source too short to hold a head at all is not a transport failure: it
  // is the memory path's OPEN_NOT_A_PACKAGE, mirrored.
  {
    std::vector<unsigned char> stub(kFixedHeadBytes - 1, 0);
    MemoryByteSource source(stub);
    Reader reader;
    std::size_t need = 0;
    LOKA_VERIFY(reader.beginOpen(source,
                            kStamp,
                            Reader::VERIFY_INTEGRITY,
                            need) == Reader::OPEN_NOT_A_PACKAGE);
  }

  // The file disagreeing with its own declared length is a fact about the
  // package, not about the transport, so it is truncation in both doors --
  // whether the extra bytes are there or the missing ones are not.
  {
    std::vector<unsigned char> longer(package);
    longer.resize(longer.size() + kPayloadAlign, 0);
    ExpectOpenResultInBothModes(longer, Reader::OPEN_TRUNCATED);
  }
  {
    std::vector<unsigned char> shorter(package);
    shorter.resize(shorter.size() - kPayloadAlign);
    ExpectOpenResultInBothModes(shorter, Reader::OPEN_TRUNCATED);
  }

  // A source that opens fine and fails on the payload. Opening cannot see it
  // -- the slice stops at the DATA header -- so the refusal belongs to the
  // bag read, and the bag stays closed.
  {
    MemoryByteSource source(package);
    source.failReadsOver(dataPayloadAt, package.size());
    std::vector<unsigned char> index;
    std::vector<unsigned char> bag;
    Reader reader;
    LOKA_VERIFY(OpenThroughStream(reader,
                             source,
                             kStamp,
                             Reader::VERIFY_INTEGRITY,
                             index) == Reader::OPEN_OK);
    LOKA_VERIFY(ReadBagIntoVector(reader, 0, bag) == Reader::BAG_SOURCE_FAILED);
    assert(!reader.isBagOpen(0));
    Facts facts;
    Asset asset;
    LOKA_VERIFY(reader.get(11, facts, asset) == Reader::GET_BAG_NOT_OPEN);

    // The same buffer may simply be retried once the source stops lying,
    // because success rewrites every byte of it.
    source.failReadsOver(0, 0);
    LOKA_VERIFY(reader.readBagInto(0, &bag[0], bag.size()) == Reader::BAG_OK);
    assert(reader.isBagOpen(0));
    LOKA_VERIFY(reader.get(11, facts, asset) == Reader::GET_OK);
    assert(AssetEquals(asset, kDefault, sizeof(kDefault)));
  }

  // A source that answers the probes honestly and then hands finishOpen a
  // slice whose INDX claims a payload past the measured span. The parse
  // trusts nothing the probes concluded except the buffer's size, so the lie
  // runs into the same resident bound the scanner applies to any package --
  // and, under the sanitized build, provably without reading past the buffer
  // the probes sized.
  {
    std::vector<unsigned char> lyingSlice(package);
    std::size_t indexSize = 0;
    const std::size_t indexHeader =
        FindChunkHeader(lyingSlice, FourCC('I', 'N', 'D', 'X'), indexSize);
    const std::size_t resident = dataPayloadAt - kFixedHeadBytes;
    const std::size_t indexPayloadRebased =
        indexHeader + kChunkHeaderBytes - kFixedHeadBytes;
    // One byte past the slice, still far inside the file's logical length.
    WriteU32BE(&lyingSlice[indexHeader + 4],
               static_cast<U32>(resident - indexPayloadRebased + 1));

    TwoFacedByteSource source(package, lyingSlice);
    std::vector<unsigned char> index;
    Reader reader;
    LOKA_VERIFY(OpenThroughStream(reader,
                             source,
                             kStamp,
                             Reader::SKIP_INTEGRITY,
                             index) == Reader::OPEN_MALFORMED_INDEX);
    assert(!reader.isOpen());
  }

  printf("==== [testLrpkStreamRefusesSourceLies] end ====\n");
}

void testLrpkReadBagIntoWalksTheSameRefusalOrder()
{
  printf("\n==== [testLrpkReadBagIntoWalksTheSameRefusalOrder] start ====\n");

  std::vector<unsigned char> package;
  LOKA_VERIFY(BuildTwoBagPackage(package) == Writer::BUILD_OK);

  // The wrong door, from either side.
  {
    Reader memory;
    LOKA_VERIFY(memory.openBorrowedBytes(&package[0],
                                    package.size(),
                                    kStamp,
                                    Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    std::size_t stored = 0;
    LOKA_VERIFY(memory.bagStoredSize(0, stored) && stored > 0);
    std::vector<unsigned char> buffer(stored, 0);
    LOKA_VERIFY(memory.readBagInto(0, &buffer[0], stored) ==
           Reader::BAG_WRONG_BACKING);
  }

  MemoryByteSource source(package);
  std::vector<unsigned char> index;
  std::vector<unsigned char> bag;
  Reader reader;
  LOKA_VERIFY(OpenThroughStream(reader,
                           source,
                           kStamp,
                           Reader::VERIFY_INTEGRITY,
                           index) == Reader::OPEN_OK);
  LOKA_VERIFY(reader.openBag(0) == Reader::BAG_WRONG_BACKING);

  std::size_t stored = 0;
  LOKA_VERIFY(reader.bagStoredSize(0, stored) && stored > 0);
  std::size_t missing = 0;
  LOKA_VERIFY(!reader.bagStoredSize(2, missing) && missing == 0 &&
         "the size query doubles as existence");

  // Refusal 2: an index no bag has, and a reader with no package at all.
  {
    std::vector<unsigned char> buffer(stored, 0);
    LOKA_VERIFY(reader.readBagInto(2, &buffer[0], stored) ==
           Reader::BAG_NO_SUCH_BAG);
    Reader unopened;
    LOKA_VERIFY(unopened.readBagInto(0, &buffer[0], stored) ==
           Reader::BAG_NO_SUCH_BAG);
  }

  // Refusal 6, from both sides, before any read happens.
  {
    std::vector<unsigned char> tooBig(stored + 1, 0);
    LOKA_VERIFY(reader.readBagInto(0, &tooBig[0], stored + 1) ==
           Reader::BAG_BUFFER_SIZE_MISMATCH);
    std::vector<unsigned char> tooSmall(stored - 1, 0);
    LOKA_VERIFY(reader.readBagInto(0, &tooSmall[0], stored - 1) ==
           Reader::BAG_BUFFER_SIZE_MISMATCH);
    assert(!reader.isBagOpen(0));
  }

  // Refusal 7. The size is right, so this cannot be refusal 6 wearing a
  // different name: only the address is wrong.
  {
    std::vector<unsigned char> shifted(stored + 1, 0);
    assert(reinterpret_cast<std::size_t>(&shifted[0]) % kPayloadAlign == 0);
    LOKA_VERIFY(reader.readBagInto(0, &shifted[1], stored) ==
           Reader::BAG_MISALIGNED_BUFFER);
    assert(!reader.isBagOpen(0));
  }

  // Refusal 3, and the way out of it. A second read into another buffer would
  // swap the bag's base under every pointer already handed out, so it is
  // refused; closing the bag withdraws those pointers and lets the same
  // buffer be used again.
  LOKA_VERIFY(ReadBagIntoVector(reader, 0, bag) == Reader::BAG_OK);
  {
    std::vector<unsigned char> other(stored, 0);
    LOKA_VERIFY(reader.readBagInto(0, &other[0], stored) ==
           Reader::BAG_ALREADY_OPEN);
  }
  reader.closeBag(0);
  assert(!reader.isBagOpen(0));
  LOKA_VERIFY(reader.readBagInto(0, &bag[0], bag.size()) == Reader::BAG_OK);
  assert(reader.isBagOpen(0));

  // Refusal 9: the bytes arrive and disagree with the check value.
  {
    std::vector<unsigned char> rotted(package);
    std::size_t dataSize = 0;
    rotted[FindChunkPayload(rotted, FourCC('D', 'A', 'T', 'A'), dataSize)] ^= 0xFF;
    MemoryByteSource rottedSource(rotted);
    std::vector<unsigned char> rottedIndex;
    std::vector<unsigned char> buffer;
    Reader rottedReader;
    LOKA_VERIFY(OpenThroughStream(rottedReader,
                             rottedSource,
                             kStamp,
                             Reader::VERIFY_INTEGRITY,
                             rottedIndex) == Reader::OPEN_OK);
    LOKA_VERIFY(ReadBagIntoVector(rottedReader, 0, buffer) ==
           Reader::BAG_CONTENTS_CORRUPT);
    assert(!rottedReader.isBagOpen(0));

    // And skipping verification is the reader's decision, not the package's:
    // the same bytes load.
    MemoryByteSource uncheckedSource(rotted);
    std::vector<unsigned char> uncheckedIndex;
    std::vector<unsigned char> loaded;
    Reader unchecked;
    LOKA_VERIFY(OpenThroughStream(unchecked,
                             uncheckedSource,
                             kStamp,
                             Reader::SKIP_INTEGRITY,
                             uncheckedIndex) == Reader::OPEN_OK);
    LOKA_VERIFY(ReadBagIntoVector(unchecked, 0, loaded) == Reader::BAG_OK);
  }

  // Refusals 4 and 5, the two the shared commit-time helper owns, reached
  // through the file-backed door.
  {
    U32 plain[kMaxAxes] = {0, 0, 0, 0};
    Writer writer;
    const std::size_t ja = writer.addBag();
    const std::size_t en = writer.addBag();
    writer.addAsset(AssetLayoutKey(""), 100, ja, ASSET_KIND_STRING, plain, kFile, sizeof(kFile));
    writer.addAsset(AssetLayoutKey(""), 100, en, ASSET_KIND_STRING, plain, kDefault, sizeof(kDefault));
    std::vector<unsigned char> shared;
    LOKA_VERIFY(writer.build(kStamp, shared) == Writer::BUILD_OK);

    MemoryByteSource sharedSource(shared);
    std::vector<unsigned char> sharedIndex;
    std::vector<unsigned char> first;
    std::vector<unsigned char> second;
    Reader sharedReader;
    LOKA_VERIFY(OpenThroughStream(sharedReader,
                             sharedSource,
                             kStamp,
                             Reader::VERIFY_INTEGRITY,
                             sharedIndex) == Reader::OPEN_OK);
    LOKA_VERIFY(ReadBagIntoVector(sharedReader, ja, first) == Reader::BAG_OK);
    LOKA_VERIFY(ReadBagIntoVector(sharedReader, en, second) ==
           Reader::BAG_ASSET_ID_CONFLICT);
    assert(!sharedReader.isBagOpen(en));
  }
  {
    std::vector<unsigned char> codec(package);
    std::size_t indexSize = 0;
    const std::size_t indexAt =
        FindChunkPayload(codec, FourCC('I', 'N', 'D', 'X'), indexSize);
    codec[indexAt + 8 + kBagCodec] = static_cast<unsigned char>(CODEC_RLE);
    RestampChunk(codec, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);

    MemoryByteSource codecSource(codec);
    std::vector<unsigned char> codecIndex;
    std::vector<unsigned char> buffer;
    Reader codecReader;
    LOKA_VERIFY(OpenThroughStream(codecReader,
                             codecSource,
                             kStamp,
                             Reader::VERIFY_INTEGRITY,
                             codecIndex) == Reader::OPEN_OK);
    LOKA_VERIFY(ReadBagIntoVector(codecReader, 0, buffer) ==
           Reader::BAG_UNSUPPORTED_CODEC);
  }

  // The size query follows the package, not the bag: closing the reader
  // takes it away.
  reader.close();
  std::size_t afterClose = 1;
  LOKA_VERIFY(!reader.bagStoredSize(0, afterClose) && afterClose == 0);
  assert(!reader.isOpen());

  printf("==== [testLrpkReadBagIntoWalksTheSameRefusalOrder] end ====\n");
}

void testLrpkStreamOpensEmptyAndZeroLengthBags()
{
  printf("\n==== [testLrpkStreamOpensEmptyAndZeroLengthBags] start ====\n");

  std::vector<unsigned char> package;
  LOKA_VERIFY(BuildEmptyBagPackage(package) == Writer::BUILD_OK);

  Reader memory;
  LOKA_VERIFY(memory.openBorrowedBytes(&package[0],
                                  package.size(),
                                  kStamp,
                                  Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  assert(memory.bagCount() == 2);
  LOKA_VERIFY(memory.openBag(0) == Reader::BAG_OK);
  LOKA_VERIFY(memory.openBag(1) == Reader::BAG_OK);

  MemoryByteSource source(package);
  std::vector<unsigned char> index;
  std::vector<unsigned char> full;
  Reader stream;
  LOKA_VERIFY(OpenThroughStream(stream,
                           source,
                           kStamp,
                           Reader::VERIFY_INTEGRITY,
                           index) == Reader::OPEN_OK);
  std::size_t emptyStored = 1;
  LOKA_VERIFY(stream.bagStoredSize(1, emptyStored) && emptyStored == 0 &&
         "a zero-sized bag is a bag, not a missing one");

  LOKA_VERIFY(ReadBagIntoVector(stream, 0, full) == Reader::BAG_OK);
  // The empty bag's whole call is (0, 0): there is no buffer to align and
  // none to size-check, but its transport read is still a refusal step.
  std::size_t indexSize = 0;
  const std::size_t indexAt =
      FindChunkPayload(package, FourCC('I', 'N', 'D', 'X'), indexSize);
  const std::size_t emptyBagRow = indexAt + 8 + 1 * kBagRowBytes;
  std::size_t dataHeaderAt = 0;
  std::size_t dataPayloadAt = 0;
  LocateDataChunk(package, dataHeaderAt, dataPayloadAt);
  const std::size_t emptyBagFileOffset =
      dataPayloadAt +
      static_cast<std::size_t>(
          ReadU32BE(&package[emptyBagRow + kBagDataOffset]));
  source.failReadsOver(emptyBagFileOffset, emptyBagFileOffset + 1);
  LOKA_VERIFY(stream.readBagInto(1, 0, 0) == Reader::BAG_SOURCE_FAILED);
  assert(!stream.isBagOpen(1));
  source.failReadsOver(0, 0);
  LOKA_VERIFY(stream.readBagInto(1, 0, 0) == Reader::BAG_OK);
  assert(stream.isBagOpen(1));

  // A zero-length asset is a legal row the writer emits, and it reports no
  // address rather than an address into nothing.
  Facts facts;
  for (std::size_t transport = 0; transport < 2; ++transport)
  {
    Reader &reader = transport == 0 ? memory : stream;
    Asset present;
    LOKA_VERIFY(reader.get(11, facts, present) == Reader::GET_OK);
    assert(AssetEquals(present, kDefault, sizeof(kDefault)));
    Asset zeroLength;
    LOKA_VERIFY(reader.get(12, facts, zeroLength) == Reader::GET_OK);
    assert(zeroLength.length == 0 && zeroLength.bytes == 0 &&
           "a zero-length asset does no arithmetic on a base it has none of");
    assert(zeroLength.bag == 0);
  }

  // The empty bag is CRC'd like any other, so a forged check value on it is
  // refused by both doors. Skipping it would make the format's check value
  // mean "usually inspected".
  {
    std::vector<unsigned char> forged(package);
    std::size_t forgedIndexSize = 0;
    const std::size_t forgedIndexAt =
        FindChunkPayload(forged, FourCC('I', 'N', 'D', 'X'), forgedIndexSize);
    const std::size_t forgedEmptyBagRow = forgedIndexAt + 8 + kBagRowBytes;
    assert(ReadU32BE(&forged[forgedEmptyBagRow + kBagStoredSize]) == 0);
    WriteU32BE(&forged[forgedEmptyBagRow + kBagCrc],
               ReadU32BE(&forged[forgedEmptyBagRow + kBagCrc]) ^ 0xFFFFFFFFUL);
    RestampChunk(forged, FourCC('I', 'N', 'D', 'X'), kHeadIndexCrc);

    Reader forgedMemory;
    LOKA_VERIFY(forgedMemory.openBorrowedBytes(&forged[0],
                                          forged.size(),
                                          kStamp,
                                          Reader::VERIFY_INTEGRITY) ==
           Reader::OPEN_OK);
    LOKA_VERIFY(forgedMemory.openBag(1) == Reader::BAG_CONTENTS_CORRUPT);

    MemoryByteSource forgedSource(forged);
    std::vector<unsigned char> forgedIndex;
    Reader forgedStream;
    LOKA_VERIFY(OpenThroughStream(forgedStream,
                             forgedSource,
                             kStamp,
                             Reader::VERIFY_INTEGRITY,
                             forgedIndex) == Reader::OPEN_OK);
    LOKA_VERIFY(forgedStream.readBagInto(1, 0, 0) == Reader::BAG_CONTENTS_CORRUPT);
    assert(!forgedStream.isBagOpen(1));
  }

  printf("==== [testLrpkStreamOpensEmptyAndZeroLengthBags] end ====\n");
}

void testBlobSealBytesFreezesSizeAndCompletion()
{
  printf("\n==== [testBlobSealBytesFreezesSizeAndCompletion] start ====\n");

  std::vector<unsigned char> package;
  LOKA_VERIFY(BuildEmptyBagPackage(package) == Writer::BUILD_OK);

  MemoryByteSource source(package);
  std::vector<unsigned char> index;
  // The blobs hold the bag bytes the reader points at, so they are declared
  // ahead of it for the same reason the index buffer is.
  Blob loaded = Blob::Create();
  Blob empty = Blob::Create();
  Reader reader;
  LOKA_VERIFY(OpenThroughStream(reader,
                           source,
                           kStamp,
                           Reader::VERIFY_INTEGRITY,
                           index) == Reader::OPEN_OK);

  // The application's whole sequence: create, size, fill in place, seal.
  // Nothing is copied -- the reason sealBytes exists rather than setBytes.
  std::size_t stored = 0;
  LOKA_VERIFY(reader.bagStoredSize(0, stored) && stored > 0);
  // Mutable while it is being filled, so the seal's withdrawal of that is a
  // transition and not a value that happened to already be there.
  loaded.setMutable(true);
  assert(loaded.isMutable() && !loaded.isCompleted());
  loaded.mutableBytes().resize(stored);
  assert(reinterpret_cast<std::size_t>(&loaded.mutableBytes()[0]) %
             kPayloadAlign ==
         0);
  LOKA_VERIFY(reader.readBagInto(0, &loaded.mutableBytes()[0], stored) ==
         Reader::BAG_OK);
  assert(loaded.size() == 0 && "the size is announced by the seal, not the fill");
  loaded.sealBytes();
  assert(loaded.size() == loaded.bytes().size());
  assert(loaded.size() == stored);
  assert(loaded.isCompleted());
  assert(!loaded.isMutable());

  // The condition the image side actually asks about: a valid blob whose
  // asset range lies inside the bytes it now reports.
  Facts facts;
  Asset asset;
  LOKA_VERIFY(reader.get(11, facts, asset) == Reader::GET_OK);
  assert(loaded.isValid());
  assert(BlobRangeIsUsable(loaded.bytes().size(),
                           asset.offsetInBag,
                           asset.length));
  assert(std::memcmp(&loaded.bytes()[asset.offsetInBag],
                     kDefault,
                     sizeof(kDefault)) == 0);

  // The empty bag takes the same path with nothing in it, and seals to a
  // completed, immutable, zero-length blob rather than to an invalid one.
  std::size_t emptyStored = 1;
  LOKA_VERIFY(reader.bagStoredSize(1, emptyStored) && emptyStored == 0);
  empty.setMutable(true);
  assert(empty.isMutable() && !empty.isCompleted());
  empty.mutableBytes().resize(emptyStored);
  LOKA_VERIFY(reader.readBagInto(1, 0, 0) == Reader::BAG_OK);
  empty.sealBytes();
  assert(empty.isValid());
  assert(empty.size() == 0 && empty.bytes().size() == 0);
  assert(empty.isCompleted());
  assert(!empty.isMutable());
  // A zero-length asset is not a decodable range, which is the image seam's
  // own rule and not a defect in the bag that carries it.
  Asset zeroLength;
  LOKA_VERIFY(reader.get(12, facts, zeroLength) == Reader::GET_OK);
  assert(!BlobRangeIsUsable(loaded.bytes().size(),
                            zeroLength.offsetInBag,
                            zeroLength.length));

  printf("==== [testBlobSealBytesFreezesSizeAndCompletion] end ====\n");
}
