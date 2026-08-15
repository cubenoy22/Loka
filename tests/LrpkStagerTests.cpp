#include "LrpkStagerTests.hpp"
#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

#include "core/resource/lrpk/LrpkFormat.hpp"
#include "core/resource/lrpk/LrpkReader.hpp"
#include "lrpc/LrpkStager.hpp"
#include "lrpc/LrpkWriter.hpp"

using namespace loka::core::resource::lrpk;
using loka::lrpc::AssetLayoutKey;
using loka::lrpc::CorruptionSite;
using loka::lrpc::StagePackageBytes;
using loka::lrpc::StagePackageResult;
using loka::lrpc::Writer;

namespace
{
  const U32 kStagerStamp = 0x51544147UL;

  std::vector<unsigned char> BuildPackage()
  {
    static const unsigned char first[] = {'f', 'i', 'r', 's', 't'};
    static const unsigned char second[] = {'s', 'e', 'c', 'o', 'n', 'd'};
    Writer writer;
    const std::size_t firstBag = writer.addBag();
    const std::size_t secondBag = writer.addBag();
    writer.addAsset(AssetLayoutKey("first"), 1, firstBag,
                    ASSET_KIND_STRING, 0, first, sizeof(first));
    writer.addAsset(AssetLayoutKey("second"), 2, secondBag,
                    ASSET_KIND_STRING, 0, second, sizeof(second));
    std::vector<unsigned char> package;
    LOKA_VERIFY(writer.build(kStagerStamp, package) == Writer::BUILD_OK);
    return package;
  }

  void AppendU32(std::vector<unsigned char> &bytes, U32 value)
  {
    const std::size_t at = bytes.size();
    bytes.resize(at + 4);
    WriteU32BE(&bytes[at], value);
  }

  void AppendChunk(std::vector<unsigned char> &bytes,
                   U32 tag,
                   const std::vector<unsigned char> &payload)
  {
    AppendU32(bytes, tag);
    AppendU32(bytes, static_cast<U32>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    while (bytes.size() % kPayloadAlign != 0)
    {
      bytes.push_back(0);
    }
  }

  std::vector<unsigned char> SyntheticPackage(
      const std::vector<unsigned char> &index,
      const std::vector<unsigned char> &data,
      bool includeIndex,
      bool includeData)
  {
    std::vector<unsigned char> package(kFixedHeadBytes, 0);
    WriteU32BE(&package[0], FourCC('L', 'R', 'P', 'K'));
    WriteU32BE(&package[kFormHeaderBytes], FourCC('H', 'E', 'A', 'D'));
    if (includeIndex)
    {
      AppendChunk(package, FourCC('I', 'N', 'D', 'X'), index);
    }
    if (includeData)
    {
      AppendChunk(package, FourCC('D', 'A', 'T', 'A'), data);
    }
    WriteU32BE(&package[4], static_cast<U32>(package.size() - kFormHeaderBytes));
    return package;
  }

  StagePackageResult Stage(const std::vector<unsigned char> &source,
                           std::size_t bag)
  {
    std::vector<unsigned char> staged;
    CorruptionSite site;
    return StagePackageBytes(source, &bag, staged, site);
  }
}

void testLrpcStagesValidatedCopyAndCorruptsOneBagByte()
{
  printf("\n==== [testLrpcStagesValidatedCopyAndCorruptsOneBagByte] start ====\n");

  const std::vector<unsigned char> package = BuildPackage();
  std::vector<unsigned char> copied;
  CorruptionSite copiedSite;
  LOKA_VERIFY(StagePackageBytes(package, 0, copied, copiedSite) ==
              loka::lrpc::STAGE_PACKAGE_OK);
  assert(copied == package);

  const std::size_t bag = 1;
  std::vector<unsigned char> corrupted;
  CorruptionSite corruptedSite;
  LOKA_VERIFY(StagePackageBytes(package, &bag, corrupted, corruptedSite) ==
              loka::lrpc::STAGE_PACKAGE_OK);
  assert(package.size() == corrupted.size());
  assert(corruptedSite.payloadStart <= corruptedSite.byteOffset);
  assert(corruptedSite.byteOffset < corruptedSite.payloadEnd);
  std::size_t changed = 0;
  for (std::size_t i = 0; i < package.size(); ++i)
  {
    if (package[i] != corrupted[i])
    {
      ++changed;
      assert(i == corruptedSite.byteOffset);
      assert(corrupted[i] == static_cast<unsigned char>(package[i] ^ 0x01));
    }
  }
  assert(changed == 1);

  Reader reader;
  LOKA_VERIFY(reader.openBorrowedBytes(&corrupted[0], corrupted.size(),
                                      kStagerStamp,
                                      Reader::VERIFY_INTEGRITY) == Reader::OPEN_OK);
  LOKA_VERIFY(reader.openBag(bag) == Reader::BAG_CONTENTS_CORRUPT);

  printf("==== [testLrpcStagesValidatedCopyAndCorruptsOneBagByte] end ====\n");
}

void testLrpcStageRefusesEveryMalformedPackageBoundary()
{
  printf("\n==== [testLrpcStageRefusesEveryMalformedPackageBoundary] start ====\n");

  const std::vector<unsigned char> package = BuildPackage();
  {
    std::vector<unsigned char> bad(12, 0);
    std::vector<unsigned char> staged(1, 0xA5);
    CorruptionSite site;
    LOKA_VERIFY(StagePackageBytes(bad, 0, staged, site) ==
                loka::lrpc::STAGE_PACKAGE_NOT_FIXED_HEAD);
    assert(staged.size() == 1 && staged[0] == 0xA5);
  }
  {
    std::vector<unsigned char> bad(package);
    WriteU32BE(&bad[0], FourCC('N', 'O', 'P', 'E'));
    LOKA_VERIFY(Stage(bad, 0) == loka::lrpc::STAGE_PACKAGE_NOT_FIXED_HEAD);
    bad = package;
    WriteU32BE(&bad[kFormHeaderBytes], FourCC('N', 'O', 'P', 'E'));
    LOKA_VERIFY(Stage(bad, 0) == loka::lrpc::STAGE_PACKAGE_NOT_FIXED_HEAD);
  }
  {
    std::vector<unsigned char> bad(package);
    WriteU32BE(&bad[4], ReadU32BE(&bad[4]) - 1);
    LOKA_VERIFY(Stage(bad, 0) ==
                loka::lrpc::STAGE_PACKAGE_FORM_LENGTH_MISMATCH);
  }
  {
    std::vector<unsigned char> bad(package);
    bad.push_back('t');
    bad.push_back('a');
    bad.push_back('i');
    bad.push_back('l');
    WriteU32BE(&bad[4], static_cast<U32>(bad.size() - kFormHeaderBytes));
    LOKA_VERIFY(Stage(bad, 0) ==
                loka::lrpc::STAGE_PACKAGE_TRUNCATED_CHUNK_HEADER);
  }
  {
    std::vector<unsigned char> bad(package);
    AppendU32(bad, FourCC('J', 'U', 'N', 'K'));
    AppendU32(bad, 4);
    WriteU32BE(&bad[4], static_cast<U32>(bad.size() - kFormHeaderBytes));
    LOKA_VERIFY(Stage(bad, 0) ==
                loka::lrpc::STAGE_PACKAGE_TRUNCATED_CHUNK_PAYLOAD);
  }
  {
    std::vector<unsigned char> index(8, 0);
    std::vector<unsigned char> data(4, 0);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, false, true), 0) ==
                loka::lrpc::STAGE_PACKAGE_MISSING_INDEX_OR_DATA);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, true, false), 0) ==
                loka::lrpc::STAGE_PACKAGE_MISSING_INDEX_OR_DATA);
  }
  {
    std::vector<unsigned char> index(4, 0);
    std::vector<unsigned char> data(4, 0);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, true, true), 0) ==
                loka::lrpc::STAGE_PACKAGE_INDEX_TOO_SHORT);
  }
  {
    std::vector<unsigned char> index(8, 0);
    std::vector<unsigned char> data(4, 0);
    WriteU32BE(&index[0], 1);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, true, true), 0) ==
                loka::lrpc::STAGE_PACKAGE_INDEX_ROW_COUNTS_MISMATCH);
    WriteU32BE(&index[0], 0);
    WriteU32BE(&index[4], 0xFFFFFFFFUL);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, true, true), 0) ==
                loka::lrpc::STAGE_PACKAGE_INDEX_ROW_COUNTS_MISMATCH);
  }
  {
    std::vector<unsigned char> index(8, 0);
    std::vector<unsigned char> data(4, 0);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, true, true), 0) ==
                loka::lrpc::STAGE_PACKAGE_BAG_OUT_OF_RANGE);
  }
  {
    std::vector<unsigned char> index(8 + kBagRowBytes, 0);
    std::vector<unsigned char> data(4, 0);
    WriteU32BE(&index[0], 1);
    WriteU32BE(&index[8 + kBagDataOffset], 1);
    WriteU32BE(&index[8 + kBagStoredSize], 1);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, true, true), 0) ==
                loka::lrpc::STAGE_PACKAGE_INVALID_BAG_PAYLOAD_BOUNDS);
    WriteU32BE(&index[8 + kBagDataOffset], 4);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, true, true), 0) ==
                loka::lrpc::STAGE_PACKAGE_INVALID_BAG_PAYLOAD_BOUNDS);
    WriteU32BE(&index[8 + kBagDataOffset], 0);
    WriteU32BE(&index[8 + kBagStoredSize], 0);
    LOKA_VERIFY(Stage(SyntheticPackage(index, data, true, true), 0) ==
                loka::lrpc::STAGE_PACKAGE_INVALID_BAG_PAYLOAD_BOUNDS);
  }

  printf("==== [testLrpcStageRefusesEveryMalformedPackageBoundary] end ====\n");
}
