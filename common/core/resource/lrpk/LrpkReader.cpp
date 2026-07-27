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
          U32 HeadCrc(const unsigned char *bytes)
          {
            Crc32 crc;
            crc.update(bytes + kFormHeaderBytes, kChunkHeaderBytes);
            crc.update(bytes + kHeadPayloadOffset + 4, kHeadPayloadBytes - 4);
            return crc.value();
          }

          U32 ChunkCrc(const unsigned char *chunkHeader, std::size_t payloadSize)
          {
            return Crc32::Of(chunkHeader, kChunkHeaderBytes + payloadSize);
          }
        } // namespace

        Reader::Reader()
            : state_()
        {
        }

        void Reader::close()
        {
          state_ = State();
        }

        Reader::OpenResult Reader::openBorrowedBytes(const unsigned char *bytes,
                                                     std::size_t size,
                                                     U32 expectedIdSpaceStamp,
                                                     IntegrityMode integrityMode)
        {
          // Parse into one completed value and commit only after every check.
          State next;
          if (!bytes || size < kFixedHeadBytes)
          {
            return OPEN_NOT_A_PACKAGE;
          }
          if (!SizeFitsU32(size))
          {
            return OPEN_SIZE_OUT_OF_RANGE;
          }
          if (ReadU32BE(bytes) != FourCC('L', 'R', 'P', 'K'))
          {
            return OPEN_NOT_A_PACKAGE;
          }
          if (ReadU32BE(bytes + 4) != static_cast<U32>(size - kFormHeaderBytes))
          {
            return OPEN_TRUNCATED;
          }
          if (ReadU32BE(bytes + kFormHeaderBytes) != FourCC('H', 'E', 'A', 'D') ||
              ReadU32BE(bytes + kFormHeaderBytes + 4) != static_cast<U32>(kHeadPayloadBytes))
          {
            return OPEN_NOT_A_PACKAGE;
          }

          const unsigned char *head = bytes + kHeadPayloadOffset;
          const bool verify = integrityMode == VERIFY_INTEGRITY;
          if (verify && HeadCrc(bytes) != ReadU32BE(head + kHeadCrc))
          {
            return OPEN_HEAD_CORRUPT;
          }
          if (ReadU32BE(head + kHeadVersion) != kFormatVersion)
          {
            return OPEN_UNSUPPORTED_VERSION;
          }
          if (ReadU32BE(head + kHeadTotalBytes) != static_cast<U32>(size))
          {
            return OPEN_TRUNCATED;
          }
          const U32 stamp = ReadU32BE(head + kHeadIdSpaceStamp);
          if (stamp != expectedIdSpaceStamp)
          {
            return OPEN_ID_SPACE_MISMATCH;
          }
          // V1 has no data-controlled verification switch. Any future meaning
          // for a flag requires a version that knows how to interpret it.
          if (ReadU32BE(head + kHeadFlags) != 0)
          {
            return OPEN_MALFORMED_INDEX;
          }

          const std::size_t declaredAssets =
              static_cast<std::size_t>(ReadU32BE(head + kHeadAssetCount));
          const std::size_t declaredBags =
              static_cast<std::size_t>(ReadU32BE(head + kHeadBagCount));

          const unsigned char *axesChunk = 0;
          const unsigned char *axesPayload = 0;
          std::size_t axesPayloadSize = 0;
          const unsigned char *indexChunk = 0;
          const unsigned char *indexPayload = 0;
          std::size_t indexPayloadSize = 0;
          const unsigned char *dataChunk = 0;

          std::size_t cursor = kFixedHeadBytes;
          while (cursor < size)
          {
            if (size - cursor < kChunkHeaderBytes)
            {
              return OPEN_TRUNCATED;
            }
            const unsigned char *chunk = bytes + cursor;
            const U32 tag = ReadU32BE(chunk);
            const std::size_t payloadSize =
                static_cast<std::size_t>(ReadU32BE(chunk + 4));
            const std::size_t payloadAt = cursor + kChunkHeaderBytes;
            if (!ExtentFits(size, payloadAt, payloadSize))
            {
              return OPEN_TRUNCATED;
            }
            const std::size_t paddedSize = AlignUp(payloadSize, kPayloadAlign);
            if (paddedSize < payloadSize || !ExtentFits(size, payloadAt, paddedSize))
            {
              return OPEN_TRUNCATED;
            }

            if (tag == FourCC('A', 'X', 'E', 'S'))
            {
              if (axesChunk)
              {
                return OPEN_MALFORMED_INDEX;
              }
              axesChunk = chunk;
              axesPayload = chunk + kChunkHeaderBytes;
              axesPayloadSize = payloadSize;
            }
            else if (tag == FourCC('I', 'N', 'D', 'X'))
            {
              if (indexChunk)
              {
                return OPEN_MALFORMED_INDEX;
              }
              indexChunk = chunk;
              indexPayload = chunk + kChunkHeaderBytes;
              indexPayloadSize = payloadSize;
            }
            else if (tag == FourCC('D', 'A', 'T', 'A'))
            {
              if (dataChunk)
              {
                return OPEN_MALFORMED_INDEX;
              }
              dataChunk = chunk;
              next.dataPayload = chunk + kChunkHeaderBytes;
              next.dataPayloadSize = payloadSize;
            }
            else
            {
              // V1's chunk set is closed. Changing one bit of a required tag
              // cannot demote it through letter case.
              return OPEN_UNKNOWN_CHUNK;
            }

            cursor = payloadAt + paddedSize;
          }

          // V1's required set is fixed by the version, including empty AXES.
          if (!axesChunk || !indexChunk || !dataChunk)
          {
            return OPEN_MALFORMED_INDEX;
          }
          if (verify)
          {
            if (ChunkCrc(axesChunk, axesPayloadSize) != ReadU32BE(head + kHeadAxesCrc) ||
                ChunkCrc(indexChunk, indexPayloadSize) != ReadU32BE(head + kHeadIndexCrc))
            {
              return OPEN_INDEX_CORRUPT;
            }
            if (Crc32::Of(dataChunk, kChunkHeaderBytes) !=
                ReadU32BE(head + kHeadDataHeaderCrc))
            {
              return OPEN_INDEX_CORRUPT;
            }
          }

          if (axesPayloadSize < 4)
          {
            return OPEN_MALFORMED_INDEX;
          }
          next.axisCount = static_cast<std::size_t>(axesPayload[0]);
          if (next.axisCount > kMaxAxes ||
              axesPayloadSize != 4 + next.axisCount * kAxisEntryBytes ||
              axesPayload[1] != 0 || axesPayload[2] != 0 || axesPayload[3] != 0)
          {
            return OPEN_MALFORMED_INDEX;
          }

          bool rankSeen[kMaxAxes] = {false, false, false, false};
          for (std::size_t a = 0; a < next.axisCount; ++a)
          {
            const unsigned char *entry = axesPayload + 4 + a * kAxisEntryBytes;
            Axis &axis = next.axes[a];
            axis.kind = entry[kAxisKind];
            axis.valueCount = entry[kAxisValueCount];
            const unsigned char precedenceRank = entry[kAxisPrecedenceRank];
            if ((axis.kind != AXIS_KIND_ENUM && axis.kind != AXIS_KIND_SCALAR) ||
                axis.valueCount > kMaxAxisValues ||
                precedenceRank >= next.axisCount ||
                rankSeen[precedenceRank])
            {
              return OPEN_MALFORMED_INDEX;
            }
            rankSeen[precedenceRank] = true;
            next.precedenceSlots[precedenceRank] = static_cast<unsigned char>(a);
            axis.baseline = ReadU32BE(entry + kAxisBaseline);
            for (std::size_t v = 0; v < axis.valueCount; ++v)
            {
              axis.values[v] = ReadU16BE(entry + kAxisValues + v * 2);
              for (std::size_t earlier = 0; earlier < v; ++earlier)
              {
                if (axis.values[earlier] == axis.values[v])
                {
                  return OPEN_MALFORMED_INDEX;
                }
              }
              if (axis.kind == AXIS_KIND_SCALAR && v > 0 &&
                  axis.values[v - 1] >= axis.values[v])
              {
                return OPEN_MALFORMED_INDEX;
              }
            }
          }

          if (indexPayloadSize < 8)
          {
            return OPEN_MALFORMED_INDEX;
          }
          next.bagCount = static_cast<std::size_t>(ReadU32BE(indexPayload));
          next.assetCount = static_cast<std::size_t>(ReadU32BE(indexPayload + 4));
          if (next.bagCount != declaredBags ||
              next.assetCount != declaredAssets ||
              next.bagCount > kMaxBags)
          {
            return OPEN_MALFORMED_INDEX;
          }

          const std::size_t rowSpace = indexPayloadSize - 8;
          if (!ProductFits(rowSpace, next.bagCount, kBagRowBytes))
          {
            return OPEN_MALFORMED_INDEX;
          }
          const std::size_t bagBytes = next.bagCount * kBagRowBytes;
          const std::size_t assetSpace = rowSpace - bagBytes;
          if (!ProductFits(assetSpace, next.assetCount, kAssetRowBytes) ||
              next.assetCount * kAssetRowBytes != assetSpace)
          {
            return OPEN_MALFORMED_INDEX;
          }

          const unsigned char *bagRows = indexPayload + 8;
          next.assetRows = bagRows + bagBytes;

          for (std::size_t b = 0; b < next.bagCount; ++b)
          {
            const unsigned char *row = bagRows + b * kBagRowBytes;
            Bag &bag = next.bags[b];
            bag.dataOffset = ReadU32BE(row + kBagDataOffset);
            bag.storedSize = ReadU32BE(row + kBagStoredSize);
            bag.expandedSize = ReadU32BE(row + kBagExpandedSize);
            bag.crc = ReadU32BE(row + kBagCrc);
            bag.codec = row[kBagCodec];
            bag.open = false;
            const std::size_t offset = static_cast<std::size_t>(bag.dataOffset);
            const std::size_t stored = static_cast<std::size_t>(bag.storedSize);
            if (!ExtentFits(next.dataPayloadSize, offset, stored) ||
                (bag.codec == CODEC_NONE && bag.expandedSize != bag.storedSize))
            {
              return OPEN_MALFORMED_INDEX;
            }
          }

          // Validate every invariant later lookup and selection depend on.
          for (std::size_t i = 0; i < next.assetCount; ++i)
          {
            const unsigned char *row = next.assetRows + i * kAssetRowBytes;
            if (i > 0 &&
                ReadU32BE(next.assetRows + (i - 1) * kAssetRowBytes + kRowId) >
                    ReadU32BE(row + kRowId))
            {
              return OPEN_MALFORMED_INDEX;
            }
            const std::size_t bagIndex = static_cast<std::size_t>(row[kRowBag]);
            if (bagIndex >= next.bagCount)
            {
              return OPEN_MALFORMED_INDEX;
            }
            if (row[kRowKind] >
                static_cast<unsigned char>(ASSET_KIND_AUDIO))
            {
              return OPEN_MALFORMED_INDEX;
            }
            if (i > 0 &&
                ReadU32BE(next.assetRows + (i - 1) * kAssetRowBytes + kRowId) ==
                    ReadU32BE(row + kRowId) &&
                next.assetRows[(i - 1) * kAssetRowBytes + kRowKind] != row[kRowKind])
            {
              return OPEN_MALFORMED_INDEX;
            }

            const U32 packed = ReadU16BE(row + kRowAxes);
            for (std::size_t a = 0; a < kMaxAxes; ++a)
            {
              const U32 index = (packed >> (4 * a)) & 0xFUL;
              if (a >= next.axisCount)
              {
                if (index != 0)
                {
                  return OPEN_MALFORMED_INDEX;
                }
                continue;
              }
              if (index > next.axes[a].valueCount)
              {
                return OPEN_MALFORMED_INDEX;
              }
              if (index != 0 && next.axes[a].kind == AXIS_KIND_SCALAR &&
                  next.axes[a].values[static_cast<std::size_t>(index - 1)] ==
                      next.axes[a].baseline)
              {
                return OPEN_MALFORMED_INDEX;
              }
            }

            const std::size_t offset =
                static_cast<std::size_t>(ReadU32BE(row + kRowOffset));
            const std::size_t length =
                static_cast<std::size_t>(ReadU32BE(row + kRowLength));
            if (!ExtentFits(static_cast<std::size_t>(next.bags[bagIndex].expandedSize),
                            offset,
                            length))
            {
              return OPEN_MALFORMED_INDEX;
            }
          }

          next.bytes = bytes;
          next.size = size;
          next.verifyIntegrity = verify;
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
          if (state_.bags[bagIndex].open)
          {
            return BAG_OK;
          }

          // Refuse ambiguity before doing any bag work. Rows are grouped by id,
          // so each run can answer whether the candidate and an open bag both
          // carry it without allocating an auxiliary set.
          std::size_t i = 0;
          while (i < state_.assetCount)
          {
            const U32 id = ReadU32BE(assetRow(i) + kRowId);
            bool inCandidate = false;
            bool inOpenBag = false;
            do
            {
              const std::size_t rowBag =
                  static_cast<std::size_t>(assetRow(i)[kRowBag]);
              inCandidate = inCandidate || rowBag == bagIndex;
              inOpenBag = inOpenBag ||
                          (rowBag != bagIndex && state_.bags[rowBag].open);
              ++i;
            } while (i < state_.assetCount &&
                     ReadU32BE(assetRow(i) + kRowId) == id);
            if (inCandidate && inOpenBag)
            {
              return BAG_ASSET_ID_CONFLICT;
            }
          }

          Bag &bag = state_.bags[bagIndex];
          if (bag.codec != CODEC_NONE)
          {
            return BAG_UNSUPPORTED_CODEC;
          }
          const unsigned char *payload =
              state_.dataPayload + static_cast<std::size_t>(bag.dataOffset);
          if (state_.verifyIntegrity &&
              Crc32::Of(payload, static_cast<std::size_t>(bag.storedSize)) != bag.crc)
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
          return isOpen() &&
                 bagIndex < state_.bagCount &&
                 state_.bags[bagIndex].open;
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
          if (low < state_.assetCount &&
              ReadU32BE(assetRow(low) + kRowId) == id)
          {
            return low;
          }
          return state_.assetCount;
        }

        bool Reader::rowIsEligible(const unsigned char *row,
                                   const Facts &facts) const
        {
          for (std::size_t a = 0; a < state_.axisCount; ++a)
          {
            const U32 written = RowAxisIndex(row, a);
            if (written == 0)
            {
              continue;
            }
            if (!facts.present[a])
            {
              return false;
            }
            if (state_.axes[a].kind == AXIS_KIND_ENUM)
            {
              bool matchesDeclaredValue = false;
              for (std::size_t v = 0; v < state_.axes[a].valueCount; ++v)
              {
                if (state_.axes[a].values[v] == facts.value[a])
                {
                  matchesDeclaredValue =
                      written == static_cast<U32>(v + 1);
                  break;
                }
              }
              if (!matchesDeclaredValue)
              {
                return false;
              }
            }
          }
          return true;
        }

        bool Reader::rowMatchesFilters(const unsigned char *row,
                                       const bool *filtered,
                                       const bool *enumRequiresWritten,
                                       const U32 *scalarValues) const
        {
          for (std::size_t a = 0; a < state_.axisCount; ++a)
          {
            if (!filtered[a])
            {
              continue;
            }
            if (state_.axes[a].kind == AXIS_KIND_ENUM)
            {
              const bool written = RowAxisIndex(row, a) != 0;
              if (written != enumRequiresWritten[a])
              {
                return false;
              }
            }
            else
            {
              if (rowScalarValue(row, a) != scalarValues[a])
              {
                return false;
              }
            }
          }
          return true;
        }

        U32 Reader::rowScalarValue(const unsigned char *row,
                                   std::size_t axis) const
        {
          const U32 index = RowAxisIndex(row, axis);
          if (index == 0)
          {
            return state_.axes[axis].baseline;
          }
          return state_.axes[axis].values[index - 1];
        }

        Reader::GetResult Reader::get(U32 id,
                                      const Facts &facts,
                                      Asset &out) const
        {
          out = Asset();
          if (!isOpen())
          {
            return GET_BAG_NOT_OPEN;
          }
          const std::size_t first = findFirstRow(id);
          if (first == state_.assetCount)
          {
            return GET_NO_SUCH_ID;
          }

          std::size_t end = first;
          bool hasOpenBagRow = false;
          while (end < state_.assetCount &&
                 ReadU32BE(assetRow(end) + kRowId) == id)
          {
            const std::size_t bag =
                static_cast<std::size_t>(assetRow(end)[kRowBag]);
            hasOpenBagRow = hasOpenBagRow || state_.bags[bag].open;
            ++end;
          }
          if (!hasOpenBagRow)
          {
            return GET_BAG_NOT_OPEN;
          }

          bool filtered[kMaxAxes] = {false, false, false, false};
          bool enumRequiresWritten[kMaxAxes] = {false, false, false, false};
          U32 scalarValues[kMaxAxes] = {0, 0, 0, 0};

          // Phase B: visit package precedence ranks, each time scanning only
          // survivors of Phase A and every earlier axis.
          for (std::size_t rank = 0; rank < state_.axisCount; ++rank)
          {
            const std::size_t axis =
                static_cast<std::size_t>(state_.precedenceSlots[rank]);
            bool foundCandidate = false;
            if (state_.axes[axis].kind == AXIS_KIND_ENUM)
            {
              bool anyWritten = false;
              for (std::size_t i = first; i < end; ++i)
              {
                const unsigned char *row = assetRow(i);
                const std::size_t bag =
                    static_cast<std::size_t>(row[kRowBag]);
                if (!state_.bags[bag].open ||
                    !rowIsEligible(row, facts) ||
                    !rowMatchesFilters(row,
                                       filtered,
                                       enumRequiresWritten,
                                       scalarValues))
                {
                  continue;
                }
                foundCandidate = true;
                anyWritten = anyWritten || RowAxisIndex(row, axis) != 0;
              }
              if (!foundCandidate)
              {
                return GET_NO_MATCHING_REP;
              }
              filtered[axis] = true;
              enumRequiresWritten[axis] = anyWritten;
            }
            else
            {
              // Phase A already removed every row that writes an absent
              // scalar axis. Its surviving rows all omit this axis, so there
              // is deliberately no second absent-value rule here.
              if (!facts.present[axis])
              {
                continue;
              }
              U32 selected = 0;
              bool selectedAtOrAbove = false;
              for (std::size_t i = first; i < end; ++i)
              {
                const unsigned char *row = assetRow(i);
                const std::size_t bag =
                    static_cast<std::size_t>(row[kRowBag]);
                if (!state_.bags[bag].open ||
                    !rowIsEligible(row, facts) ||
                    !rowMatchesFilters(row,
                                       filtered,
                                       enumRequiresWritten,
                                       scalarValues))
                {
                  continue;
                }
                const U32 value = rowScalarValue(row, axis);
                const U32 wanted = facts.value[axis];
                const bool atOrAbove = value >= wanted;
                if (!foundCandidate ||
                    (atOrAbove && !selectedAtOrAbove) ||
                    (atOrAbove == selectedAtOrAbove &&
                     ((atOrAbove && value < selected) ||
                      (!atOrAbove && value > selected))))
                {
                  selected = value;
                  selectedAtOrAbove = atOrAbove;
                }
                foundCandidate = true;
              }
              if (!foundCandidate)
              {
                return GET_NO_MATCHING_REP;
              }
              filtered[axis] = true;
              scalarValues[axis] = selected;
            }
          }

          // After Phase B there must be exactly one survivor. A forged
          // package can bypass lrpc's ambiguity wall; physical row order is
          // never a winner rule.
          const unsigned char *best = 0;
          for (std::size_t i = first; i < end; ++i)
          {
            const unsigned char *row = assetRow(i);
            const std::size_t bag =
                static_cast<std::size_t>(row[kRowBag]);
            if (!state_.bags[bag].open ||
                !rowIsEligible(row, facts) ||
                !rowMatchesFilters(row,
                                   filtered,
                                   enumRequiresWritten,
                                   scalarValues))
            {
              continue;
            }
            if (best)
            {
              return GET_NO_MATCHING_REP;
            }
            best = row;
          }
          if (!best)
          {
            return GET_NO_MATCHING_REP;
          }

          const std::size_t bag =
              static_cast<std::size_t>(best[kRowBag]);
          const std::size_t offset =
              static_cast<std::size_t>(ReadU32BE(best + kRowOffset));
          out.bytes = state_.dataPayload +
                      static_cast<std::size_t>(state_.bags[bag].dataOffset) +
                      offset;
          out.length =
              static_cast<std::size_t>(ReadU32BE(best + kRowLength));
          out.kind = static_cast<AssetKind>(best[kRowKind]);
          return GET_OK;
        }
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka
