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
          const std::size_t kHeadAxesCrc = 28;

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
            : state_()
        {
        }

        void Reader::close()
        {
          state_ = State();
        }

        Reader::OpenResult Reader::openBorrowedBytes(const unsigned char *bytes, std::size_t size, U32 expectedIdSpaceStamp)
        {
          // Parsed into `next` and committed only on success, so a refused
          // reload leaves a reader that already held a good package untouched,
          // open bags included.
          State next;
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
          next.flags = ReadU32BE(head + kHeadFlags);
          const std::size_t declaredAssets = static_cast<std::size_t>(ReadU32BE(head + kHeadAssetCount));
          const std::size_t declaredBags = static_cast<std::size_t>(ReadU32BE(head + kHeadBagCount));
          if (declaredBags > kMaxBags)
          {
            return OPEN_MALFORMED_INDEX;
          }

          const unsigned char *indexPayload = 0;
          std::size_t indexPayloadSize = 0;
          const unsigned char *axesPayload = 0;
          std::size_t axesPayloadSize = 0;

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
              axesPayload = payload;
              axesPayloadSize = payloadSize;
              next.axisCount = static_cast<std::size_t>(payload[0]);
              if (next.axisCount > kMaxAxes || payloadSize < 4 + next.axisCount * kAxisEntryBytes)
              {
                return OPEN_MALFORMED_INDEX;
              }
              for (std::size_t a = 0; a < next.axisCount; ++a)
              {
                const unsigned char *entry = payload + 4 + a * kAxisEntryBytes;
                next.axes[a].kind = entry[kAxisKind];
                // An unknown kind would be skipped by both the enum filter and
                // the scalar ranking, letting a row that matches nothing win on
                // the specificity tie-break alone.
                if (next.axes[a].kind != AXIS_KIND_ENUM && next.axes[a].kind != AXIS_KIND_SCALAR)
                {
                  return OPEN_MALFORMED_INDEX;
                }
                next.axes[a].valueCount = entry[kAxisValueCount];
                if (next.axes[a].valueCount > kMaxAxisValues)
                {
                  return OPEN_MALFORMED_INDEX;
                }
                next.axes[a].baseline = ReadU32BE(entry + kAxisBaseline);
                for (std::size_t v = 0; v < next.axes[a].valueCount; ++v)
                {
                  next.axes[a].values[v] = ReadU16BE(entry + kAxisValues + v * 2);
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
              next.dataPayload = payload;
              next.dataPayloadSize = payloadSize;
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
          const bool crcPresent = (next.flags & kFlagHasCrc) != 0;
          if (crcPresent && Crc32::Of(indexPayload, indexPayloadSize) != indexCrc)
          {
            return OPEN_INDEX_CORRUPT;
          }

          if (indexPayloadSize < 8)
          {
            return OPEN_MALFORMED_INDEX;
          }
          next.bagCount = static_cast<std::size_t>(ReadU32BE(indexPayload));
          next.assetCount = static_cast<std::size_t>(ReadU32BE(indexPayload + 4));
          if (next.bagCount != declaredBags || next.assetCount != declaredAssets || next.bagCount > kMaxBags)
          {
            return OPEN_MALFORMED_INDEX;
          }
          // Sized by division, never by multiplying a declared count: a forged
          // assetCount of 0x10000000 makes `count * 16` wrap to zero, and an
          // eight-byte payload would then satisfy a naive comparison while
          // every later binary search reads far outside the buffer.
          const std::size_t rowSpace = indexPayloadSize - 8;
          if (indexPayloadSize < 8 || !ProductFits(rowSpace, next.bagCount, kBagRowBytes))
          {
            return OPEN_MALFORMED_INDEX;
          }
          const std::size_t assetSpace = rowSpace - next.bagCount * kBagRowBytes;
          if (!ProductFits(assetSpace, next.assetCount, kAssetRowBytes))
          {
            return OPEN_MALFORMED_INDEX;
          }

          next.bagRows = indexPayload + 8;
          next.assetRows = next.bagRows + next.bagCount * kBagRowBytes;

          // Ascending id is a format invariant, and get() relies on it: an
          // unsorted table would make the binary search report GET_NO_SUCH_ID
          // for an id that is present, which is a lie rather than a refusal.
          // A reader may not assume its input honours an invariant it depends
          // on, so it is checked here rather than trusted.
          for (std::size_t i = 1; i < next.assetCount; ++i)
          {
            if (ReadU32BE(next.assetRows + (i - 1) * kAssetRowBytes + kRowId) >
                ReadU32BE(next.assetRows + i * kAssetRowBytes + kRowId))
            {
              return OPEN_MALFORMED_INDEX;
            }
          }

          for (std::size_t b = 0; b < next.bagCount; ++b)
          {
            const unsigned char *row = next.bagRows + b * kBagRowBytes;
            next.bags[b].dataOffset = ReadU32BE(row + kBagDataOffset);
            next.bags[b].storedSize = ReadU32BE(row + kBagStoredSize);
            next.bags[b].expandedSize = ReadU32BE(row + kBagExpandedSize);
            next.bags[b].crc = ReadU32BE(row + kBagCrc);
            next.bags[b].codec = row[kBagCodec];
            next.bags[b].open = false;
            // Bounds are checked by subtraction, never by adding two untrusted
            // 32-bit fields: on a 32-bit target their sum can wrap to a small
            // value and pass a naive comparison.
            const std::size_t offset = static_cast<std::size_t>(next.bags[b].dataOffset);
            const std::size_t stored = static_cast<std::size_t>(next.bags[b].storedSize);
            if (!next.dataPayload || !ExtentFits(next.dataPayloadSize, offset, stored))
            {
              return OPEN_MALFORMED_INDEX;
            }
            // While the codec is none the bag is not expanded, so a larger
            // expanded size would let a row be bounded against bytes that were
            // never stored -- and get() hands back a pointer into the stored
            // payload, so those bytes belong to the next bag or to nothing.
            if (next.bags[b].codec == CODEC_NONE && next.bags[b].expandedSize != next.bags[b].storedSize)
            {
              return OPEN_MALFORMED_INDEX;
            }
          }

          // AXES decides which representation is served, so leaving it out of
          // the checked metadata would let a bit flip there change the picture
          // silently while every recorded CRC still matched.
          if (crcPresent)
          {
            const U32 axesCrc = ReadU32BE(head + kHeadAxesCrc);
            if (Crc32::Of(axesPayload, axesPayloadSize) != axesCrc)
            {
              return OPEN_INDEX_CORRUPT;
            }
          }

          next.bytes = bytes;
          next.size = size;
          next.idSpaceStamp = stamp;
          state_ = next;
          return OPEN_OK;
        }

        Reader::BagResult Reader::openBag(std::size_t bagIndex)
        {
          if (!isOpen() || bagIndex >= state_.bagCount)
          {
            return BAG_NO_SUCH_BAG;
          }
          Bag &bag = state_.bags[bagIndex];
          if (bag.codec != CODEC_NONE)
          {
            return BAG_UNSUPPORTED_CODEC;
          }
          const unsigned char *payload = state_.dataPayload + bag.dataOffset;
          if (hasCrc() && Crc32::Of(payload, static_cast<std::size_t>(bag.storedSize)) != bag.crc)
          {
            return BAG_CONTENTS_CORRUPT;
          }
          bag.open = true;
          return BAG_OK;
        }

        void Reader::closeBag(std::size_t bagIndex)
        {
          if (isOpen() && bagIndex < state_.bagCount)
          {
            state_.bags[bagIndex].open = false;
          }
        }

        bool Reader::isBagOpen(std::size_t bagIndex) const
        {
          return isOpen() && bagIndex < state_.bagCount && state_.bags[bagIndex].open;
        }

        const unsigned char *Reader::assetRow(std::size_t index) const
        {
          return state_.assetRows + index * kAssetRowBytes;
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
          std::size_t high = state_.assetCount;
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
          if (low < state_.assetCount && ReadU32BE(assetRow(low) + kRowId) == id)
          {
            return low;
          }
          return state_.assetCount;
        }

        bool Reader::rowSurvivesEnumAxes(const unsigned char *row, const Facts &facts) const
        {
          for (std::size_t a = 0; a < state_.axisCount; ++a)
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
            if (state_.axes[a].kind == AXIS_KIND_ENUM && facts.value[a] != written)
            {
              return false;
            }
          }
          return true;
        }

        U32 Reader::rowScalarValue(const unsigned char *row, std::size_t axis, bool &written) const
        {
          const U32 index = RowAxisIndex(row, axis);
          if (index == 0 || index > state_.axes[axis].valueCount)
          {
            written = false;
            return state_.axes[axis].baseline;
          }
          written = true;
          return state_.axes[axis].values[index - 1];
        }

        Reader::GetResult Reader::get(U32 id, const Facts &facts, Asset &out) const
        {
          out = Asset();
          if (!isOpen())
          {
            return GET_NO_SUCH_ID;
          }
          const std::size_t first = findFirstRow(id);
          if (first == state_.assetCount)
          {
            return GET_NO_SUCH_ID;
          }

          const unsigned char *best = 0;
          bool sawRowInClosedBag = false;

          for (std::size_t i = first; i < state_.assetCount; ++i)
          {
            const unsigned char *row = assetRow(i);
            if (ReadU32BE(row + kRowId) != id)
            {
              break;
            }
            // First filter: only rows in an open bag are candidates.
            const std::size_t bag = static_cast<std::size_t>(row[kRowBag]);
            if (bag >= state_.bagCount || !state_.bags[bag].open)
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
            for (std::size_t a = 0; a < state_.axisCount && !decided; ++a)
            {
              if (state_.axes[a].kind != AXIS_KIND_SCALAR)
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
              const U32 want = facts.present[a] ? facts.value[a] : state_.axes[a].baseline;
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
          const std::size_t expanded = static_cast<std::size_t>(state_.bags[bag].expandedSize);
          if (!ExtentFits(expanded, offset, length))
          {
            return GET_NO_MATCHING_REP;
          }
          out.bytes = state_.dataPayload + state_.bags[bag].dataOffset + offset;
          out.length = length;
          out.kind = static_cast<AssetKind>(best[kRowKind]);
          return GET_OK;
        }
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka
