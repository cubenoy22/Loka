#include "core/resource/lrpk/LrpkReader.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      namespace lrpk
      {
        namespace
        {
          const std::size_t kFormHeaderBytes = 8;
          const std::size_t kChunkHeaderBytes = 8;
          const std::size_t kHeadPayloadBytes = kFixedHeadBytes - kFormHeaderBytes - kChunkHeaderBytes;

          // HEAD payload field offsets.
          const std::size_t kHeadVersion = 0;
          const std::size_t kHeadTotalBytes = 4;
          const std::size_t kHeadIdSpaceStamp = 8;
          const std::size_t kHeadIndexCrc = 12;
          const std::size_t kHeadFlags = 16;
          const std::size_t kHeadAssetCount = 20;
          const std::size_t kHeadBagCount = 24;

          // One AXES entry is fixed width so the reader does no arithmetic on
          // untrusted lengths to find the next one.
          const std::size_t kAxisEntryBytes = 40;
          const std::size_t kAxisKind = 0;
          const std::size_t kAxisValueCount = 1;
          const std::size_t kAxisBaseline = 4;
          const std::size_t kAxisValues = 8;

          // Bag row field offsets.
          const std::size_t kBagDataOffset = 0;
          const std::size_t kBagStoredSize = 4;
          const std::size_t kBagExpandedSize = 8;
          const std::size_t kBagCrc = 12;
          const std::size_t kBagCodec = 16;

          // Asset row field offsets.
          const std::size_t kRowId = 0;
          const std::size_t kRowOffset = 4;
          const std::size_t kRowLength = 8;
          const std::size_t kRowBag = 12;
          const std::size_t kRowKind = 13;
          const std::size_t kRowAxes = 14;

          const std::size_t kMaxBags = 16;
        } // namespace

        Reader::Reader()
            : bytes_(0),
              size_(0),
              flags_(0),
              idSpaceStamp_(0),
              dataPayload_(0),
              dataPayloadSize_(0),
              bagRows_(0),
              assetRows_(0),
              bagCount_(0),
              assetCount_(0),
              axisCount_(0)
        {
        }

        void Reader::close()
        {
          bytes_ = 0;
          size_ = 0;
          flags_ = 0;
          idSpaceStamp_ = 0;
          dataPayload_ = 0;
          dataPayloadSize_ = 0;
          bagRows_ = 0;
          assetRows_ = 0;
          bagCount_ = 0;
          assetCount_ = 0;
          axisCount_ = 0;
          for (std::size_t i = 0; i < kMaxBags; ++i)
          {
            bags_[i] = Bag();
          }
          for (std::size_t i = 0; i < kMaxAxes; ++i)
          {
            axes_[i] = Axis();
          }
        }

        Reader::OpenResult Reader::openFromMemory(const unsigned char *bytes, std::size_t size, U32 expectedIdSpaceStamp)
        {
          close();
          if (!bytes || size < kFixedHeadBytes)
          {
            return OPEN_NOT_A_PACKAGE;
          }
          if (ReadU32BE(bytes) != FourCC('L', 'R', 'P', 'K'))
          {
            return OPEN_NOT_A_PACKAGE;
          }
          if (ReadU32BE(bytes + kFormHeaderBytes) != FourCC('H', 'E', 'A', 'D'))
          {
            return OPEN_NOT_A_PACKAGE;
          }
          if (ReadU32BE(bytes + kFormHeaderBytes + 4) != static_cast<U32>(kHeadPayloadBytes))
          {
            return OPEN_NOT_A_PACKAGE;
          }

          const unsigned char *head = bytes + kFixedHeadBytes - kHeadPayloadBytes;
          if (ReadU32BE(head + kHeadVersion) != kFormatVersion)
          {
            return OPEN_UNSUPPORTED_VERSION;
          }
          // Cheapest and most effective check: the recorded total against the
          // actual size catches truncation and appending with one comparison.
          if (ReadU32BE(head + kHeadTotalBytes) != static_cast<U32>(size))
          {
            return OPEN_TRUNCATED;
          }
          // Checked before any CRC: holding another build's package is the
          // failure that happens daily and whose symptom is silent.
          const U32 stamp = ReadU32BE(head + kHeadIdSpaceStamp);
          if (stamp != expectedIdSpaceStamp)
          {
            return OPEN_ID_SPACE_MISMATCH;
          }

          const U32 indexCrc = ReadU32BE(head + kHeadIndexCrc);
          flags_ = ReadU32BE(head + kHeadFlags);
          const std::size_t declaredAssets = static_cast<std::size_t>(ReadU32BE(head + kHeadAssetCount));
          const std::size_t declaredBags = static_cast<std::size_t>(ReadU32BE(head + kHeadBagCount));
          if (declaredBags > kMaxBags)
          {
            return OPEN_MALFORMED_INDEX;
          }

          const unsigned char *indexPayload = 0;
          std::size_t indexPayloadSize = 0;

          std::size_t cursor = kFixedHeadBytes;
          while (cursor + kChunkHeaderBytes <= size)
          {
            const U32 fourCC = ReadU32BE(bytes + cursor);
            const std::size_t payloadSize = static_cast<std::size_t>(ReadU32BE(bytes + cursor + 4));
            const unsigned char *payload = bytes + cursor + kChunkHeaderBytes;
            if (payloadSize > size - (cursor + kChunkHeaderBytes))
            {
              return OPEN_TRUNCATED;
            }

            if (fourCC == FourCC('A', 'X', 'E', 'S'))
            {
              if (payloadSize < 4)
              {
                return OPEN_MALFORMED_INDEX;
              }
              axisCount_ = static_cast<std::size_t>(payload[0]);
              if (axisCount_ > kMaxAxes || payloadSize < 4 + axisCount_ * kAxisEntryBytes)
              {
                return OPEN_MALFORMED_INDEX;
              }
              for (std::size_t a = 0; a < axisCount_; ++a)
              {
                const unsigned char *entry = payload + 4 + a * kAxisEntryBytes;
                axes_[a].kind = entry[kAxisKind];
                axes_[a].valueCount = entry[kAxisValueCount];
                if (axes_[a].valueCount > kMaxAxisValues)
                {
                  return OPEN_MALFORMED_INDEX;
                }
                axes_[a].baseline = ReadU32BE(entry + kAxisBaseline);
                for (std::size_t v = 0; v < axes_[a].valueCount; ++v)
                {
                  axes_[a].values[v] = ReadU16BE(entry + kAxisValues + v * 2);
                }
              }
            }
            else if (fourCC == FourCC('I', 'N', 'D', 'X'))
            {
              indexPayload = payload;
              indexPayloadSize = payloadSize;
            }
            else if (fourCC == FourCC('D', 'A', 'T', 'A'))
            {
              dataPayload_ = payload;
              dataPayloadSize_ = payloadSize;
            }
            else if (IsCriticalChunk(fourCC))
            {
              // Uppercase and unknown. Skipping unconditionally would let a
              // required chunk go silently missing.
              return OPEN_UNKNOWN_CRITICAL_CHUNK;
            }

            cursor += kChunkHeaderBytes + AlignUp(payloadSize, kPayloadAlign);
          }

          if (!indexPayload)
          {
            return OPEN_MALFORMED_INDEX;
          }
          if (hasCrc() && Crc32::Of(indexPayload, indexPayloadSize) != indexCrc)
          {
            return OPEN_INDEX_CORRUPT;
          }

          if (indexPayloadSize < 8)
          {
            return OPEN_MALFORMED_INDEX;
          }
          bagCount_ = static_cast<std::size_t>(ReadU32BE(indexPayload));
          assetCount_ = static_cast<std::size_t>(ReadU32BE(indexPayload + 4));
          if (bagCount_ != declaredBags || assetCount_ != declaredAssets || bagCount_ > kMaxBags)
          {
            return OPEN_MALFORMED_INDEX;
          }
          const std::size_t needed = 8 + bagCount_ * kBagRowBytes + assetCount_ * kAssetRowBytes;
          if (indexPayloadSize < needed)
          {
            return OPEN_MALFORMED_INDEX;
          }

          bagRows_ = indexPayload + 8;
          assetRows_ = bagRows_ + bagCount_ * kBagRowBytes;

          for (std::size_t b = 0; b < bagCount_; ++b)
          {
            const unsigned char *row = bagRows_ + b * kBagRowBytes;
            bags_[b].dataOffset = ReadU32BE(row + kBagDataOffset);
            bags_[b].storedSize = ReadU32BE(row + kBagStoredSize);
            bags_[b].expandedSize = ReadU32BE(row + kBagExpandedSize);
            bags_[b].crc = ReadU32BE(row + kBagCrc);
            bags_[b].codec = row[kBagCodec];
            bags_[b].open = false;
            const std::size_t end =
                static_cast<std::size_t>(bags_[b].dataOffset) + static_cast<std::size_t>(bags_[b].storedSize);
            if (!dataPayload_ || end > dataPayloadSize_)
            {
              return OPEN_MALFORMED_INDEX;
            }
          }

          bytes_ = bytes;
          size_ = size;
          idSpaceStamp_ = stamp;
          return OPEN_OK;
        }

        Reader::BagResult Reader::openBag(std::size_t bagIndex)
        {
          if (!isOpen() || bagIndex >= bagCount_)
          {
            return BAG_NO_SUCH_BAG;
          }
          Bag &bag = bags_[bagIndex];
          if (bag.codec != CODEC_NONE)
          {
            return BAG_UNSUPPORTED_CODEC;
          }
          const unsigned char *payload = dataPayload_ + bag.dataOffset;
          if (hasCrc() && Crc32::Of(payload, static_cast<std::size_t>(bag.storedSize)) != bag.crc)
          {
            return BAG_CONTENTS_CORRUPT;
          }
          bag.open = true;
          return BAG_OK;
        }

        void Reader::closeBag(std::size_t bagIndex)
        {
          if (isOpen() && bagIndex < bagCount_)
          {
            bags_[bagIndex].open = false;
          }
        }

        bool Reader::isBagOpen(std::size_t bagIndex) const
        {
          return isOpen() && bagIndex < bagCount_ && bags_[bagIndex].open;
        }

        const unsigned char *Reader::assetRow(std::size_t index) const
        {
          return assetRows_ + index * kAssetRowBytes;
        }

        U32 Reader::RowAxisIndex(const unsigned char *row, std::size_t axis)
        {
          const U32 packed = ReadU16BE(row + kRowAxes);
          return (packed >> (4 * axis)) & 0xFUL;
        }

        std::size_t Reader::RowWrittenAxisCount(const unsigned char *row)
        {
          std::size_t count = 0;
          for (std::size_t a = 0; a < kMaxAxes; ++a)
          {
            if (RowAxisIndex(row, a) != 0)
            {
              ++count;
            }
          }
          return count;
        }

        std::size_t Reader::findFirstRow(U32 id) const
        {
          std::size_t low = 0;
          std::size_t high = assetCount_;
          while (low < high)
          {
            const std::size_t mid = low + (high - low) / 2;
            if (ReadU32BE(assetRow(mid) + kRowId) < id)
            {
              low = mid + 1;
            }
            else
            {
              high = mid;
            }
          }
          if (low < assetCount_ && ReadU32BE(assetRow(low) + kRowId) == id)
          {
            return low;
          }
          return assetCount_;
        }

        bool Reader::rowSurvivesEnumAxes(const unsigned char *row, const Facts &facts) const
        {
          for (std::size_t a = 0; a < axisCount_; ++a)
          {
            const U32 written = RowAxisIndex(row, a);
            if (written == 0)
            {
              // The row says nothing about this axis, so it cannot disagree.
              continue;
            }
            if (!facts.present[a])
            {
              // What cannot be decided is not a match.
              return false;
            }
            if (axes_[a].kind == AXIS_KIND_ENUM && facts.value[a] != written)
            {
              return false;
            }
          }
          return true;
        }

        U32 Reader::rowScalarValue(const unsigned char *row, std::size_t axis, bool &written) const
        {
          const U32 index = RowAxisIndex(row, axis);
          if (index == 0 || index > axes_[axis].valueCount)
          {
            written = false;
            return axes_[axis].baseline;
          }
          written = true;
          return axes_[axis].values[index - 1];
        }

        Reader::GetResult Reader::get(U32 id, const Facts &facts, Asset &out) const
        {
          out = Asset();
          if (!isOpen())
          {
            return GET_NO_SUCH_ID;
          }
          const std::size_t first = findFirstRow(id);
          if (first == assetCount_)
          {
            return GET_NO_SUCH_ID;
          }

          const unsigned char *best = 0;
          bool sawRowInClosedBag = false;

          for (std::size_t i = first; i < assetCount_; ++i)
          {
            const unsigned char *row = assetRow(i);
            if (ReadU32BE(row + kRowId) != id)
            {
              break;
            }
            // First filter: only rows in an open bag are candidates.
            const std::size_t bag = static_cast<std::size_t>(row[kRowBag]);
            if (bag >= bagCount_ || !bags_[bag].open)
            {
              sawRowInClosedBag = true;
              continue;
            }
            if (!rowSurvivesEnumAxes(row, facts))
            {
              continue;
            }
            if (!best)
            {
              best = row;
              continue;
            }

            // Scalar axes decide between survivors: the smallest at or above
            // the requested value, and if none is at or above it, the largest.
            bool decided = false;
            for (std::size_t a = 0; a < axisCount_ && !decided; ++a)
            {
              if (axes_[a].kind != AXIS_KIND_SCALAR)
              {
                continue;
              }
              bool bestWritten = false;
              bool rowWritten = false;
              const U32 bestValue = rowScalarValue(best, a, bestWritten);
              const U32 rowValue = rowScalarValue(row, a, rowWritten);
              if (bestValue == rowValue)
              {
                continue;
              }
              const U32 want = facts.present[a] ? facts.value[a] : axes_[a].baseline;
              const bool bestAtOrAbove = bestValue >= want;
              const bool rowAtOrAbove = rowValue >= want;
              if (bestAtOrAbove != rowAtOrAbove)
              {
                if (rowAtOrAbove)
                {
                  best = row;
                }
                decided = true;
              }
              else if (bestAtOrAbove)
              {
                if (rowValue < bestValue)
                {
                  best = row;
                }
                decided = true;
              }
              else
              {
                if (rowValue > bestValue)
                {
                  best = row;
                }
                decided = true;
              }
            }
            if (decided)
            {
              continue;
            }
            // Tie: the more specific row wins.
            if (RowWrittenAxisCount(row) > RowWrittenAxisCount(best))
            {
              best = row;
            }
          }

          if (!best)
          {
            return sawRowInClosedBag ? GET_BAG_NOT_OPEN : GET_NO_MATCHING_REP;
          }

          const std::size_t bag = static_cast<std::size_t>(best[kRowBag]);
          const std::size_t offset = static_cast<std::size_t>(ReadU32BE(best + kRowOffset));
          const std::size_t length = static_cast<std::size_t>(ReadU32BE(best + kRowLength));
          if (offset + length > static_cast<std::size_t>(bags_[bag].expandedSize))
          {
            return GET_NO_MATCHING_REP;
          }
          out.bytes = dataPayload_ + bags_[bag].dataOffset + offset;
          out.length = length;
          out.kind = static_cast<AssetKind>(best[kRowKind]);
          return GET_OK;
        }
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka
