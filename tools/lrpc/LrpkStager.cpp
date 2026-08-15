#include "lrpc/LrpkStager.hpp"

#include "core/resource/lrpk/LrpkFormat.hpp"

namespace loka
{
  namespace lrpc
  {
    using namespace core::resource::lrpk;

    namespace
    {
      struct ChunkView
      {
        ChunkView()
            : found(false), payloadStart(0), payloadSize(0)
        {
        }

        bool found;
        std::size_t payloadStart;
        std::size_t payloadSize;
      };

      StagePackageResult FindCorruptionSite(
          const std::vector<unsigned char> &source,
          std::size_t targetBag,
          CorruptionSite &site)
      {
        if (source.size() < kFixedHeadBytes ||
            ReadU32BE(&source[0]) != FourCC('L', 'R', 'P', 'K') ||
            ReadU32BE(&source[kFormHeaderBytes]) != FourCC('H', 'E', 'A', 'D'))
        {
          return STAGE_PACKAGE_NOT_FIXED_HEAD;
        }
        if (static_cast<std::size_t>(ReadU32BE(&source[4])) !=
            source.size() - kFormHeaderBytes)
        {
          return STAGE_PACKAGE_FORM_LENGTH_MISMATCH;
        }

        ChunkView index;
        ChunkView data;
        std::size_t cursor = kFixedHeadBytes;
        while (cursor < source.size())
        {
          if (!ExtentFits(source.size(), cursor, kChunkHeaderBytes))
          {
            return STAGE_PACKAGE_TRUNCATED_CHUNK_HEADER;
          }
          const U32 tag = ReadU32BE(&source[cursor]);
          const std::size_t payloadSize =
              static_cast<std::size_t>(ReadU32BE(&source[cursor + 4]));
          const std::size_t payloadStart = cursor + kChunkHeaderBytes;
          if (!ExtentFits(source.size(), payloadStart, payloadSize))
          {
            return STAGE_PACKAGE_TRUNCATED_CHUNK_PAYLOAD;
          }
          const std::size_t padding =
              payloadSize % kPayloadAlign == 0
                  ? 0
                  : kPayloadAlign - payloadSize % kPayloadAlign;
          if (!ExtentFits(source.size(), payloadStart + payloadSize, padding))
          {
            return STAGE_PACKAGE_TRUNCATED_CHUNK_PAYLOAD;
          }
          if (tag == FourCC('I', 'N', 'D', 'X'))
          {
            index.found = true;
            index.payloadStart = payloadStart;
            index.payloadSize = payloadSize;
          }
          else if (tag == FourCC('D', 'A', 'T', 'A'))
          {
            data.found = true;
            data.payloadStart = payloadStart;
            data.payloadSize = payloadSize;
          }
          cursor = payloadStart + payloadSize + padding;
        }

        if (!index.found || !data.found)
        {
          return STAGE_PACKAGE_MISSING_INDEX_OR_DATA;
        }
        if (index.payloadSize < 8)
        {
          return STAGE_PACKAGE_INDEX_TOO_SHORT;
        }

        const unsigned char *indexBytes = &source[index.payloadStart];
        const std::size_t bagCount =
            static_cast<std::size_t>(ReadU32BE(indexBytes));
        const std::size_t assetCount =
            static_cast<std::size_t>(ReadU32BE(indexBytes + 4));
        const std::size_t rowsSize = index.payloadSize - 8;
        if (!ProductFits(rowsSize, bagCount, kBagRowBytes))
        {
          return STAGE_PACKAGE_INDEX_ROW_COUNTS_MISMATCH;
        }
        const std::size_t bagRowsSize = bagCount * kBagRowBytes;
        if (!ProductFits(rowsSize - bagRowsSize,
                         assetCount,
                         kAssetRowBytes) ||
            bagRowsSize + assetCount * kAssetRowBytes != rowsSize)
        {
          return STAGE_PACKAGE_INDEX_ROW_COUNTS_MISMATCH;
        }
        if (targetBag >= bagCount)
        {
          return STAGE_PACKAGE_BAG_OUT_OF_RANGE;
        }

        const std::size_t bagRow = 8 + targetBag * kBagRowBytes;
        const std::size_t dataOffset = static_cast<std::size_t>(
            ReadU32BE(indexBytes + bagRow + kBagDataOffset));
        const std::size_t storedSize = static_cast<std::size_t>(
            ReadU32BE(indexBytes + bagRow + kBagStoredSize));
        if (dataOffset % kPayloadAlign != 0 || storedSize == 0 ||
            !ExtentFits(data.payloadSize, dataOffset, storedSize))
        {
          return STAGE_PACKAGE_INVALID_BAG_PAYLOAD_BOUNDS;
        }

        const std::size_t payloadStart = data.payloadStart + dataOffset;
        const std::size_t payloadEnd = payloadStart + storedSize;
        const std::size_t byteOffset = payloadStart + storedSize / 2;
        const std::size_t dataEnd = data.payloadStart + data.payloadSize;
        if (payloadStart < data.payloadStart || byteOffset < payloadStart ||
            byteOffset >= payloadEnd || payloadEnd > dataEnd ||
            dataEnd > source.size())
        {
          return STAGE_PACKAGE_CORRUPTION_BYTE_OUT_OF_BOUNDS;
        }

        site.payloadStart = payloadStart;
        site.payloadEnd = payloadEnd;
        site.byteOffset = byteOffset;
        return STAGE_PACKAGE_OK;
      }
    } // namespace

    CorruptionSite::CorruptionSite()
        : payloadStart(0), payloadEnd(0), byteOffset(0)
    {
    }

    StagePackageResult StagePackageBytes(
        const std::vector<unsigned char> &source,
        const std::size_t *corruptBag,
        std::vector<unsigned char> &staged,
        CorruptionSite &site)
    {
      const std::size_t validationBag = corruptBag ? *corruptBag : 0;
      CorruptionSite found;
      const StagePackageResult result =
          FindCorruptionSite(source, validationBag, found);
      if (result != STAGE_PACKAGE_OK)
      {
        return result;
      }

      std::vector<unsigned char> completed(source);
      if (corruptBag)
      {
        completed[found.byteOffset] ^= 0x01;
      }
      staged.swap(completed);
      site = found;
      return STAGE_PACKAGE_OK;
    }
  } // namespace lrpc
} // namespace loka
