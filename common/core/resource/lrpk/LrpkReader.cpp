#include "core/resource/lrpk/LrpkReader.hpp"

#include <cassert>

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
          discardPending();
          state_ = State();
        }

        Reader::OpenResult Reader::ValidateHead(const unsigned char *head512,
                                                std::size_t totalSize,
                                                U32 expectedIdSpaceStamp,
                                                bool verify,
                                                U32 &stamp,
                                                std::size_t &declaredAssets,
                                                std::size_t &declaredBags)
        {
          stamp = 0;
          declaredAssets = 0;
          declaredBags = 0;

          if (ReadU32BE(head512) != FourCC('L', 'R', 'P', 'K'))
          {
            return OPEN_NOT_A_PACKAGE;
          }
          if (ReadU32BE(head512 + 4) !=
              static_cast<U32>(totalSize - kFormHeaderBytes))
          {
            return OPEN_TRUNCATED;
          }
          if (ReadU32BE(head512 + kFormHeaderBytes) != FourCC('H', 'E', 'A', 'D') ||
              ReadU32BE(head512 + kFormHeaderBytes + 4) !=
                  static_cast<U32>(kHeadPayloadBytes))
          {
            return OPEN_NOT_A_PACKAGE;
          }

          const unsigned char *head = head512 + kHeadPayloadOffset;
          if (verify && HeadCrc(head512) != ReadU32BE(head + kHeadCrc))
          {
            return OPEN_HEAD_CORRUPT;
          }
          if (ReadU32BE(head + kHeadVersion) != kFormatVersion)
          {
            return OPEN_UNSUPPORTED_VERSION;
          }
          if (ReadU32BE(head + kHeadTotalBytes) != static_cast<U32>(totalSize))
          {
            return OPEN_TRUNCATED;
          }
          stamp = ReadU32BE(head + kHeadIdSpaceStamp);
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

          declaredAssets =
              static_cast<std::size_t>(ReadU32BE(head + kHeadAssetCount));
          declaredBags =
              static_cast<std::size_t>(ReadU32BE(head + kHeadBagCount));
          return OPEN_OK;
        }

        Reader::OpenResult Reader::ParseChunkStream(State &next,
                                                    const unsigned char *base,
                                                    std::size_t resident,
                                                    std::size_t logical,
                                                    const unsigned char *headPayload,
                                                    bool verify,
                                                    std::size_t declaredAssets,
                                                    std::size_t declaredBags)
        {
          const unsigned char *axesChunk = 0;
          const unsigned char *axesPayload = 0;
          std::size_t axesPayloadSize = 0;
          const unsigned char *indexChunk = 0;
          const unsigned char *indexPayload = 0;
          std::size_t indexPayloadSize = 0;
          const unsigned char *dataChunk = 0;

          // Chunk order is canonical: AXES, then INDX, then DATA. The
          // file-backed open depends on it -- everything before the DATA
          // payload must exist as one contiguous prefix of the file, which a
          // permuted package cannot provide without a second grammar. Two
          // checks pin the whole order: INDX refuses when AXES has not been
          // established, DATA refuses when INDX has not, and DATA closes the
          // stream. With duplicates refused and the tag set closed, no other
          // out-of-turn shape is reachable.
          std::size_t cursor = 0;
          while (cursor < logical)
          {
            if (resident - cursor < kChunkHeaderBytes)
            {
              return OPEN_TRUNCATED;
            }
            const unsigned char *chunk = base + cursor;
            const U32 tag = ReadU32BE(chunk);
            const std::size_t payloadSize =
                static_cast<std::size_t>(ReadU32BE(chunk + 4));
            const std::size_t payloadAt = cursor + kChunkHeaderBytes;
            if (!ExtentFits(logical, payloadAt, payloadSize))
            {
              return OPEN_TRUNCATED;
            }
            const std::size_t paddedSize = AlignUp(payloadSize, kPayloadAlign);
            if (paddedSize < payloadSize || !ExtentFits(logical, payloadAt, paddedSize))
            {
              return OPEN_TRUNCATED;
            }

            if (tag == FourCC('A', 'X', 'E', 'S'))
            {
              if (axesChunk || !ExtentFits(resident, payloadAt, payloadSize))
              {
                return OPEN_MALFORMED_INDEX;
              }
              axesChunk = chunk;
              axesPayload = chunk + kChunkHeaderBytes;
              axesPayloadSize = payloadSize;
            }
            else if (tag == FourCC('I', 'N', 'D', 'X'))
            {
              if (!axesChunk || indexChunk ||
                  !ExtentFits(resident, payloadAt, payloadSize))
              {
                return OPEN_MALFORMED_INDEX;
              }
              indexChunk = chunk;
              indexPayload = chunk + kChunkHeaderBytes;
              indexPayloadSize = payloadSize;
            }
            else if (tag == FourCC('D', 'A', 'T', 'A'))
            {
              // No duplicate test here, unlike AXES and INDX: DATA closes the
              // chunk stream, so the loop cannot reach a second one. The test
              // that used to be here was unreachable code pretending to be a
              // rule.
              if (!indexChunk)
              {
                return OPEN_MALFORMED_INDEX;
              }
              // Checked, not refused: unlike its three siblings, this one
              // inspects our own arithmetic rather than a value the file
              // claims. The scan starts at a kPayloadAlign-aligned origin and
              // every step adds kChunkHeaderBytes plus a kPayloadAlign-rounded
              // payload, all multiples of kPayloadAlign, so no package --
              // forged or rotted -- can reach a misaligned payload start. A
              // typed refusal here would be a rule the format does not have.
              // The reachable alignment gates stay where the file gets a say:
              // the borrowed base, a bag's dataOffset, and a row's offset.
              assert(payloadAt % kPayloadAlign == 0
                     && "DATA payload start is aligned by the scan's own arithmetic");
              dataChunk = chunk;
              // Always recorded, resident or not: the stream-backed reader
              // never holds the payload and still has to seek to it.
              next.dataPayloadFileOffset = kFixedHeadBytes + payloadAt;
              if (ExtentFits(resident, payloadAt, payloadSize))
              {
                next.dataPayload = chunk + kChunkHeaderBytes;
              }
              next.dataPayloadSize = payloadSize;
              // DATA is terminal, so its end is the stream's end -- an
              // equality, refusing a short DATA and trailing bytes alike.
              // Whichever transport is walking, everything after this point
              // is unread, which is exactly what lets the file-backed open
              // stop its slice at the DATA header.
              if (payloadAt + paddedSize != logical)
              {
                return OPEN_MALFORMED_INDEX;
              }
              break;
            }
            else
            {
              // V1's chunk set is closed. Changing one bit of a required tag
              // cannot demote it through letter case.
              return OPEN_UNKNOWN_CHUNK;
            }

            // Checked, not refused: the next turn dereferences `base +
            // cursor`, so the step has to land inside what is actually there
            // -- and no input, not even a source that answers the probes and
            // the slice read differently, can make it miss. Memory-backed,
            // the two bounds are one value and the loop's own extent check
            // already bounded the padded span. Stream-backed, each branch
            // above bounded its payload by resident before reaching here,
            // and resident and payloadAt are both kPayloadAlign-aligned
            // (resident is a sum of aligned chunk spans), so the padding
            // cannot cross a boundary the payload did not. A typed refusal
            // here would be a rule the format does not have.
            assert(ExtentFits(resident, payloadAt, paddedSize) &&
                   "the scan's own bounds keep every step inside the resident bytes");
            cursor = payloadAt + paddedSize;
          }

          // V1's required set is fixed by the version, including empty AXES.
          if (!axesChunk || !indexChunk || !dataChunk)
          {
            return OPEN_MALFORMED_INDEX;
          }
          if (verify)
          {
            if (ChunkCrc(axesChunk, axesPayloadSize) !=
                    ReadU32BE(headPayload + kHeadAxesCrc) ||
                ChunkCrc(indexChunk, indexPayloadSize) !=
                    ReadU32BE(headPayload + kHeadIndexCrc))
            {
              return OPEN_INDEX_CORRUPT;
            }
            if (Crc32::Of(dataChunk, kChunkHeaderBytes) !=
                ReadU32BE(headPayload + kHeadDataHeaderCrc))
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
              if (axis.kind == AXIS_KIND_SCALAR &&
                  axis.values[v] == axis.baseline)
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
            if (offset % kPayloadAlign != 0 ||
                !ExtentFits(next.dataPayloadSize, offset, stored) ||
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
            if (offset % kPayloadAlign != 0 ||
                !ExtentFits(static_cast<std::size_t>(next.bags[bagIndex].expandedSize),
                            offset,
                            length))
            {
              return OPEN_MALFORMED_INDEX;
            }
          }

          return OPEN_OK;
        }

        Reader::OpenResult Reader::openBorrowedBytes(const unsigned char *bytes,
                                                     std::size_t size,
                                                     U32 expectedIdSpaceStamp,
                                                     IntegrityMode integrityMode)
        {
          // Starting any open leaves at most one pending, and this is not it.
          discardPending();

          // Parse into one completed value and commit only after every check.
          State next;
          if (!bytes || size < kFixedHeadBytes)
          {
            return OPEN_NOT_A_PACKAGE;
          }
          if (reinterpret_cast<std::size_t>(bytes) % kPayloadAlign != 0)
          {
            return OPEN_MISALIGNED_BUFFER;
          }
          if (!SizeFitsU32(size))
          {
            return OPEN_SIZE_OUT_OF_RANGE;
          }

          const bool verify = integrityMode == VERIFY_INTEGRITY;
          U32 stamp = 0;
          std::size_t declaredAssets = 0;
          std::size_t declaredBags = 0;
          const OpenResult headResult = ValidateHead(bytes,
                                                     size,
                                                     expectedIdSpaceStamp,
                                                     verify,
                                                     stamp,
                                                     declaredAssets,
                                                     declaredBags);
          if (headResult != OPEN_OK)
          {
            return headResult;
          }

          // Everything is here, so the two bounds coincide.
          const std::size_t resident = size - kFixedHeadBytes;
          const OpenResult parsed = ParseChunkStream(next,
                                                     bytes + kFixedHeadBytes,
                                                     resident,
                                                     resident,
                                                     bytes + kHeadPayloadOffset,
                                                     verify,
                                                     declaredAssets,
                                                     declaredBags);
          if (parsed != OPEN_OK)
          {
            return parsed;
          }

          next.backing = BACKING_MEMORY;
          next.bytes = bytes;
          next.size = size;
          next.indexBase = bytes + kFixedHeadBytes;
          next.indexSize = resident;
          next.verifyIntegrity = verify;
          next.idSpaceStamp = stamp;
          state_ = next;
          return OPEN_OK;
        }

        Reader::OpenResult Reader::ProbeChunkHeader(ByteSource &src,
                                                    std::size_t at,
                                                    std::size_t logical,
                                                    U32 expectedTag,
                                                    std::size_t &spanOut)
        {
          spanOut = 0;
          // The resident scanner stops looping at `cursor == logical` and then
          // finds a required chunk missing, so that is what "the stream ended
          // here" has to answer -- not truncation, which is the case just
          // below where a header is started but not finished.
          if (at >= logical)
          {
            return OPEN_MALFORMED_INDEX;
          }
          if (logical - at < kChunkHeaderBytes)
          {
            return OPEN_TRUNCATED;
          }

          unsigned char header[kChunkHeaderBytes];
          if (!src.readAt(kFixedHeadBytes + at, header, kChunkHeaderBytes))
          {
            return OPEN_SOURCE_FAILED;
          }

          const U32 tag = ReadU32BE(header);
          if (tag != expectedTag)
          {
            // A tag V1 knows, in the wrong place, is an order violation; the
            // scanner reaches the same verdict through its established-so-far
            // tests. Anything else is a chunk this version does not have.
            if (tag == FourCC('A', 'X', 'E', 'S') ||
                tag == FourCC('I', 'N', 'D', 'X') ||
                tag == FourCC('D', 'A', 'T', 'A'))
            {
              return OPEN_MALFORMED_INDEX;
            }
            return OPEN_UNKNOWN_CHUNK;
          }

          const std::size_t payloadSize =
              static_cast<std::size_t>(ReadU32BE(header + 4));
          const std::size_t payloadAt = at + kChunkHeaderBytes;
          if (!ExtentFits(logical, payloadAt, payloadSize))
          {
            return OPEN_TRUNCATED;
          }
          const std::size_t paddedSize = AlignUp(payloadSize, kPayloadAlign);
          if (paddedSize < payloadSize || !ExtentFits(logical, payloadAt, paddedSize))
          {
            return OPEN_TRUNCATED;
          }
          // Bounded by the extent test above, so the caller can add spans
          // without a second overflow rule: at + spanOut <= logical.
          spanOut = kChunkHeaderBytes + paddedSize;
          return OPEN_OK;
        }

        Reader::OpenResult Reader::beginOpen(ByteSource &src,
                                             U32 expectedIdSpaceStamp,
                                             IntegrityMode integrityMode,
                                             std::size_t &indexBytesNeeded)
        {
          discardPending();
          indexBytesNeeded = 0;

          std::size_t srcSize = 0;
          if (!src.size(srcSize))
          {
            return OPEN_SOURCE_FAILED;
          }
          // The two bounds the memory path applies to a borrowed buffer,
          // applied to a file, so the same package gets the same answer
          // whichever door it comes through.
          if (srcSize < kFixedHeadBytes)
          {
            return OPEN_NOT_A_PACKAGE;
          }
          if (!SizeFitsU32(srcSize))
          {
            return OPEN_SIZE_OUT_OF_RANGE;
          }

          unsigned char head[kFixedHeadBytes];
          if (!src.readAt(0, head, kFixedHeadBytes))
          {
            return OPEN_SOURCE_FAILED;
          }

          const bool verify = integrityMode == VERIFY_INTEGRITY;
          U32 stamp = 0;
          std::size_t declaredAssets = 0;
          std::size_t declaredBags = 0;
          const OpenResult headResult = ValidateHead(head,
                                                     srcSize,
                                                     expectedIdSpaceStamp,
                                                     verify,
                                                     stamp,
                                                     declaredAssets,
                                                     declaredBags);
          if (headResult != OPEN_OK)
          {
            return headResult;
          }

          // Measure, do not trust: the sizes probed here decide how big a
          // buffer to ask for and nothing else. finishOpen re-derives every
          // structural fact from the bytes it actually receives, so a source
          // that answers differently the second time can only make itself
          // refused.
          const std::size_t logical = srcSize - kFixedHeadBytes;
          std::size_t axesSpan = 0;
          OpenResult probed = ProbeChunkHeader(src,
                                               0,
                                               logical,
                                               FourCC('A', 'X', 'E', 'S'),
                                               axesSpan);
          if (probed != OPEN_OK)
          {
            return probed;
          }
          std::size_t indexSpan = 0;
          probed = ProbeChunkHeader(src,
                                    axesSpan,
                                    logical,
                                    FourCC('I', 'N', 'D', 'X'),
                                    indexSpan);
          if (probed != OPEN_OK)
          {
            return probed;
          }

          // The slice ends at the DATA payload, so it needs DATA's header and
          // not one byte more. Each span is bounded by logical, so these sums
          // cannot wrap.
          const std::size_t dataAt = axesSpan + indexSpan;
          if (dataAt >= logical)
          {
            return OPEN_MALFORMED_INDEX;
          }
          if (logical - dataAt < kChunkHeaderBytes)
          {
            return OPEN_TRUNCATED;
          }

          pending_.source = &src;
          pending_.verify = verify;
          pending_.stamp = stamp;
          pending_.totalBytes = srcSize;
          pending_.indexBytesNeeded = dataAt + kChunkHeaderBytes;
          pending_.declaredAssets = declaredAssets;
          pending_.declaredBags = declaredBags;
          for (std::size_t i = 0; i < kFixedHeadBytes; ++i)
          {
            pending_.head[i] = head[i];
          }
          indexBytesNeeded = pending_.indexBytesNeeded;
          return OPEN_OK;
        }

        Reader::OpenResult Reader::finishOpen(unsigned char *indexBuffer,
                                              std::size_t indexBufferSize)
        {
          if (!pending_.source)
          {
            return OPEN_NO_PENDING;
          }
          // One shot. Consuming the pending before any check means a refused
          // finish cannot be retried against a head that was read a while ago
          // and may no longer describe the file; the retry starts at
          // beginOpen, which reads it again.
          const Pending pending = pending_;
          discardPending();

          if (indexBufferSize != pending.indexBytesNeeded)
          {
            return OPEN_INDEX_BUFFER_SIZE_MISMATCH;
          }
          // An asserted contract, not a refusal: the size came from this
          // reader one call ago, so a missing buffer is an application bug
          // and the file has no say in it.
          assert((pending.indexBytesNeeded == 0 || indexBuffer != 0) &&
                 "finishOpen needs the buffer beginOpen asked for");
          // Parsing into the live package's own index would overwrite the
          // bytes the committed state points at -- and a refused reload is
          // supposed to leave that state untouched.
          assert((indexBufferSize == 0 || state_.indexSize == 0 ||
                  state_.indexBase == 0 ||
                  indexBuffer + indexBufferSize <= state_.indexBase ||
                  state_.indexBase + state_.indexSize <= indexBuffer) &&
                 "the index buffer must not overlap the committed package's index");

          if (!pending.source->readAt(kFixedHeadBytes,
                                      indexBuffer,
                                      pending.indexBytesNeeded))
          {
            return OPEN_SOURCE_FAILED;
          }

          State next;
          const OpenResult parsed =
              ParseChunkStream(next,
                               indexBuffer,
                               pending.indexBytesNeeded,
                               pending.totalBytes - kFixedHeadBytes,
                               pending.head + kHeadPayloadOffset,
                               pending.verify,
                               pending.declaredAssets,
                               pending.declaredBags);
          if (parsed != OPEN_OK)
          {
            return parsed;
          }

          next.backing = BACKING_STREAM;
          next.source = pending.source;
          next.bytes = 0;
          next.size = pending.totalBytes;
          next.indexBase = indexBuffer;
          next.indexSize = pending.indexBytesNeeded;
          next.verifyIntegrity = pending.verify;
          next.idSpaceStamp = pending.stamp;
          state_ = next;
          return OPEN_OK;
        }

        Reader::BagResult Reader::refuseBagConflictOrCodec(
            std::size_t bagIndex) const
        {
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

          if (state_.bags[bagIndex].codec != CODEC_NONE)
          {
            return BAG_UNSUPPORTED_CODEC;
          }
          return BAG_OK;
        }

        Reader::BagResult Reader::openBag(std::size_t bagIndex)
        {
          if (state_.backing == BACKING_STREAM)
          {
            return BAG_WRONG_BACKING;
          }
          if (!isOpen() || bagIndex >= state_.bagCount)
          {
            return BAG_NO_SUCH_BAG;
          }
          if (state_.bags[bagIndex].open)
          {
            return BAG_OK;
          }
          const BagResult shared = refuseBagConflictOrCodec(bagIndex);
          if (shared != BAG_OK)
          {
            return shared;
          }

          Bag &bag = state_.bags[bagIndex];
          const unsigned char *payload =
              state_.dataPayload + static_cast<std::size_t>(bag.dataOffset);
          if (state_.verifyIntegrity &&
              Crc32::Of(payload, static_cast<std::size_t>(bag.storedSize)) != bag.crc)
          {
            return BAG_CONTENTS_CORRUPT;
          }
          state_.bagBase[bagIndex] = payload;
          bag.open = true;
          return BAG_OK;
        }

        Reader::BagResult Reader::readBagInto(std::size_t bagIndex,
                                              unsigned char *dst,
                                              std::size_t dstSize)
        {
          if (state_.backing == BACKING_MEMORY)
          {
            return BAG_WRONG_BACKING;
          }
          if (!isOpen() || bagIndex >= state_.bagCount)
          {
            return BAG_NO_SUCH_BAG;
          }
          if (state_.bags[bagIndex].open)
          {
            return BAG_ALREADY_OPEN;
          }
          const BagResult shared = refuseBagConflictOrCodec(bagIndex);
          if (shared != BAG_OK)
          {
            return shared;
          }

          Bag &bag = state_.bags[bagIndex];
          const std::size_t stored = static_cast<std::size_t>(bag.storedSize);
          if (dstSize != stored)
          {
            return BAG_BUFFER_SIZE_MISMATCH;
          }
          if (stored > 0)
          {
            assert(dst != 0 &&
                   "a non-empty bag needs the buffer bagStoredSize asked for");
            if (reinterpret_cast<std::size_t>(dst) % kPayloadAlign != 0)
            {
              return BAG_MISALIGNED_BUFFER;
            }
            assert(state_.source &&
                   "a stream-backed package holds the source it committed with");
            // Bounded by the parse: dataPayloadFileOffset + dataOffset +
            // stored is at most the package's total length, which fits
            // std::size_t because opening checked it does.
            if (!state_.source->readAt(state_.dataPayloadFileOffset +
                                           static_cast<std::size_t>(bag.dataOffset),
                                       dst,
                                       stored))
            {
              return BAG_SOURCE_FAILED;
            }
          }
          // Empty bags are verified too. Crc32 of nothing is a defined value
          // the writer records like any other, and skipping it would make the
          // check value mean "usually inspected".
          if (state_.verifyIntegrity && Crc32::Of(dst, stored) != bag.crc)
          {
            return BAG_CONTENTS_CORRUPT;
          }
          // An empty bag installs a null base: there is no address to hand
          // out, and get() knows not to do arithmetic on one.
          state_.bagBase[bagIndex] = stored > 0 ? dst : 0;
          bag.open = true;
          return BAG_OK;
        }

        bool Reader::bagStoredSize(std::size_t bagIndex, std::size_t &out) const
        {
          out = 0;
          if (!isOpen() || bagIndex >= state_.bagCount)
          {
            return false;
          }
          out = static_cast<std::size_t>(state_.bags[bagIndex].storedSize);
          return true;
        }

        void Reader::closeBag(std::size_t bagIndex)
        {
          if (isOpen() && bagIndex < state_.bagCount)
          {
            state_.bags[bagIndex].open = false;
            state_.bagBase[bagIndex] = 0;
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
          out.length =
              static_cast<std::size_t>(ReadU32BE(best + kRowLength));
          // The truth the caller can hold on to across a reload of its own
          // ledger: which bag, and where inside it.
          out.bag = bag;
          out.offsetInBag = offset;
          // The derived view. One resolution path for every backing: the
          // bag's installed base. Null arithmetic is not performed for a
          // zero-length asset -- an empty bag installs a null base, and
          // zero-length rows are legal.
          out.bytes = out.length == 0 ? 0 : state_.bagBase[bag] + offset;
          out.kind = static_cast<AssetKind>(best[kRowKind]);
          return GET_OK;
        }
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka
