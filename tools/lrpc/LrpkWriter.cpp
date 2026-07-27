#include "lrpc/LrpkWriter.hpp"

namespace loka
{
  namespace lrpc
  {
    using namespace loka::core::resource::lrpk;

    namespace
    {
      void AppendU16(std::vector<unsigned char> &out, U32 value)
      {
        out.push_back(static_cast<unsigned char>((value >> 8) & 0xFFUL));
        out.push_back(static_cast<unsigned char>(value & 0xFFUL));
      }

      void AppendU32(std::vector<unsigned char> &out, U32 value)
      {
        out.push_back(static_cast<unsigned char>((value >> 24) & 0xFFUL));
        out.push_back(static_cast<unsigned char>((value >> 16) & 0xFFUL));
        out.push_back(static_cast<unsigned char>((value >> 8) & 0xFFUL));
        out.push_back(static_cast<unsigned char>(value & 0xFFUL));
      }

      void PadTo(std::vector<unsigned char> &out, std::size_t alignment)
      {
        while (out.size() % alignment != 0)
        {
          out.push_back(0);
        }
      }

      U32 ChunkCrc(U32 tag, const std::vector<unsigned char> &payload)
      {
        unsigned char header[kChunkHeaderBytes];
        WriteU32BE(header, tag);
        WriteU32BE(header + 4, static_cast<U32>(payload.size()));
        Crc32 crc;
        crc.update(header, kChunkHeaderBytes);
        if (!payload.empty())
        {
          crc.update(&payload[0], payload.size());
        }
        return crc.value();
      }

      U32 ChunkHeaderCrc(U32 tag, std::size_t payloadSize)
      {
        unsigned char header[kChunkHeaderBytes];
        WriteU32BE(header, tag);
        WriteU32BE(header + 4, static_cast<U32>(payloadSize));
        return Crc32::Of(header, kChunkHeaderBytes);
      }
    } // namespace

    Writer::Writer()
        : axes_(),
          precedence_(),
          rows_(),
          bagCount_(0)
    {
    }

    void Writer::declareAxis(AxisKind kind, U32 baseline, const U32 *values, std::size_t valueCount)
    {
      Axis axis;
      axis.kind = kind;
      axis.baseline = baseline;
      for (std::size_t i = 0; i < valueCount; ++i)
      {
        axis.values.push_back(values[i]);
      }
      axes_.push_back(axis);
    }

    void Writer::setRepresentationPrecedence(const std::size_t *axisSlots, std::size_t axisCount)
    {
      precedence_.clear();
      for (std::size_t i = 0; axisSlots && i < axisCount; ++i)
      {
        precedence_.push_back(axisSlots[i]);
      }
    }

    std::size_t Writer::addBag()
    {
      return bagCount_++;
    }

    void Writer::addAsset(U32 id,
                          std::size_t bag,
                          AssetKind kind,
                          const U32 *axisValueIndex,
                          const unsigned char *bytes,
                          std::size_t length)
    {
      Row row;
      row.id = id;
      row.bag = bag;
      row.kind = kind;
      for (std::size_t a = 0; a < kMaxAxes; ++a)
      {
        // Retain every caller-supplied slot. A non-zero value in a slot the
        // package never declared is an error, not a value to silently drop.
        row.axisIndex[a] = axisValueIndex ? axisValueIndex[a] : 0;
      }
      if (bytes && length > 0)
      {
        row.bytes.assign(bytes, bytes + length);
      }
      rows_.push_back(row);
    }

    Writer::BuildResult Writer::build(U32 idSpaceStamp, std::vector<unsigned char> &out) const
    {
      // Build into a completed temporary. A refusal leaves the caller's
      // previously good package untouched.
      std::vector<unsigned char> built;
      if (axes_.size() > kMaxAxes)
      {
        return BUILD_TOO_MANY_AXES;
      }
      if (bagCount_ > kMaxBags)
      {
        return BUILD_TOO_MANY_BAGS;
      }

      unsigned char rankBySlot[kMaxAxes] = {0, 0, 0, 0};
      if (precedence_.empty())
      {
        if (axes_.size() > 1)
        {
          return BUILD_BAD_PRECEDENCE;
        }
      }
      else
      {
        if (precedence_.size() != axes_.size())
        {
          return BUILD_BAD_PRECEDENCE;
        }
        bool seen[kMaxAxes] = {false, false, false, false};
        for (std::size_t rank = 0; rank < precedence_.size(); ++rank)
        {
          const std::size_t slot = precedence_[rank];
          if (slot >= axes_.size() || seen[slot])
          {
            return BUILD_BAD_PRECEDENCE;
          }
          seen[slot] = true;
          rankBySlot[slot] = static_cast<unsigned char>(rank);
        }
      }

      for (std::size_t a = 0; a < axes_.size(); ++a)
      {
        if (axes_[a].kind != AXIS_KIND_ENUM && axes_[a].kind != AXIS_KIND_SCALAR)
        {
          return BUILD_BAD_AXIS_KIND;
        }
        if (axes_[a].values.size() > kMaxAxisValues)
        {
          return BUILD_TOO_MANY_AXIS_VALUES;
        }
        for (std::size_t v = 0; v < axes_[a].values.size(); ++v)
        {
          if (axes_[a].values[v] > 0xFFFFUL)
          {
            return BUILD_AXIS_VALUE_OUT_OF_RANGE;
          }
          for (std::size_t earlier = 0; earlier < v; ++earlier)
          {
            if (axes_[a].values[earlier] == axes_[a].values[v])
            {
              return BUILD_BAD_AXIS_VOCABULARY;
            }
          }
          if (axes_[a].kind == AXIS_KIND_SCALAR && v > 0 &&
              axes_[a].values[v - 1] >= axes_[a].values[v])
          {
            return BUILD_BAD_AXIS_VOCABULARY;
          }
        }
      }

      std::vector<std::size_t> order;
      std::vector<U32> packed(rows_.size(), 0);
      for (std::size_t i = 0; i < rows_.size(); ++i)
      {
        if (rows_[i].bag >= bagCount_)
        {
          return BUILD_BAD_BAG_REFERENCE;
        }
        if (!SizeFitsU32(rows_[i].bytes.size()))
        {
          return BUILD_SIZE_OUT_OF_RANGE;
        }
        for (std::size_t a = 0; a < kMaxAxes; ++a)
        {
          if (a >= axes_.size())
          {
            if (rows_[i].axisIndex[a] != 0)
            {
              return BUILD_BAD_AXIS_REFERENCE;
            }
            continue;
          }
          if (rows_[i].axisIndex[a] > axes_[a].values.size() ||
              rows_[i].axisIndex[a] > kMaxAxisValues)
          {
            return BUILD_BAD_AXIS_REFERENCE;
          }
          if (rows_[i].axisIndex[a] != 0)
          {
            const U32 value = axes_[a].values[static_cast<std::size_t>(rows_[i].axisIndex[a] - 1)];
            if (axes_[a].kind == AXIS_KIND_SCALAR && value == axes_[a].baseline)
            {
              return BUILD_SCALAR_BASELINE_EXPLICIT;
            }
            packed[i] |= (rows_[i].axisIndex[a] & 0xFUL) << (4 * a);
          }
        }
        order.push_back(i);
      }

      // Canonical bytes are sorted by id, then encoded axes, then bag.
      for (std::size_t i = 0; i + 1 < order.size(); ++i)
      {
        std::size_t best = i;
        for (std::size_t j = i + 1; j < order.size(); ++j)
        {
          const Row &candidate = rows_[order[j]];
          const Row &current = rows_[order[best]];
          const U32 candidateAxes = packed[order[j]];
          const U32 currentAxes = packed[order[best]];
          if (candidate.id < current.id ||
              (candidate.id == current.id &&
               (candidateAxes < currentAxes ||
                (candidateAxes == currentAxes && candidate.bag < current.bag))))
          {
            best = j;
          }
        }
        const std::size_t swap = order[i];
        order[i] = order[best];
        order[best] = swap;
      }

      for (std::size_t i = 0; i + 1 < order.size(); ++i)
      {
        const Row &a = rows_[order[i]];
        const Row &b = rows_[order[i + 1]];
        if (a.id == b.id && a.bag == b.bag && packed[order[i]] == packed[order[i + 1]])
        {
          return BUILD_SELECTOR_AMBIGUOUS;
        }
      }

      // A pair with equal effective values on every selector axis can only be
      // distinguished by row order, which is forbidden. This deliberately
      // compares selector meaning rather than packed representation.
      for (std::size_t i = 0; i < order.size(); ++i)
      {
        for (std::size_t j = i + 1; j < order.size(); ++j)
        {
          const Row &a = rows_[order[i]];
          const Row &b = rows_[order[j]];
          if (a.id != b.id || a.bag != b.bag)
          {
            continue;
          }
          bool same = true;
          for (std::size_t axis = 0; axis < axes_.size(); ++axis)
          {
            const U32 ai = a.axisIndex[axis];
            const U32 bi = b.axisIndex[axis];
            if (axes_[axis].kind == AXIS_KIND_ENUM)
            {
              same = same && ai == bi;
            }
            else
            {
              const U32 av = ai == 0 ? axes_[axis].baseline
                                     : axes_[axis].values[static_cast<std::size_t>(ai - 1)];
              const U32 bv = bi == 0 ? axes_[axis].baseline
                                     : axes_[axis].values[static_cast<std::size_t>(bi - 1)];
              same = same && av == bv;
            }
          }
          if (same)
          {
            return BUILD_SELECTOR_AMBIGUOUS;
          }
        }
      }

      // Every (id, bag) needs an axis-free row so Phase A always has a
      // fallback when the bag is open.
      for (std::size_t i = 0; i < order.size(); ++i)
      {
        const U32 id = rows_[order[i]].id;
        const std::size_t bag = rows_[order[i]].bag;
        bool hasDefault = false;
        for (std::size_t j = 0; j < order.size(); ++j)
        {
          if (rows_[order[j]].id == id && rows_[order[j]].bag == bag && packed[order[j]] == 0)
          {
            hasDefault = true;
            break;
          }
        }
        if (!hasDefault)
        {
          return BUILD_ASSET_WITHOUT_DEFAULT_ROW;
        }
      }

      for (std::size_t i = 0; i + 1 < order.size(); ++i)
      {
        if (rows_[order[i]].id == rows_[order[i + 1]].id &&
            rows_[order[i]].kind != rows_[order[i + 1]].kind)
        {
          return BUILD_ASSET_KIND_MISMATCH;
        }
      }

      // Lay bags out in DATA and keep all on-disk geometry within U32.
      const std::size_t u32MaxSize = static_cast<std::size_t>(kU32Mask);
      std::vector<unsigned char> data;
      std::vector<U32> bagOffset(bagCount_, 0);
      std::vector<U32> bagSize(bagCount_, 0);
      std::vector<U32> rowOffset(rows_.size(), 0);
      for (std::size_t b = 0; b < bagCount_; ++b)
      {
        if (!SizeFitsU32(data.size()))
        {
          return BUILD_SIZE_OUT_OF_RANGE;
        }
        bagOffset[b] = static_cast<U32>(data.size());
        for (std::size_t i = 0; i < order.size(); ++i)
        {
          const Row &row = rows_[order[i]];
          if (row.bag != b)
          {
            continue;
          }
          if (!ExtentFits(u32MaxSize, data.size(), row.bytes.size()))
          {
            return BUILD_SIZE_OUT_OF_RANGE;
          }
          rowOffset[order[i]] = static_cast<U32>(data.size() - static_cast<std::size_t>(bagOffset[b]));
          data.insert(data.end(), row.bytes.begin(), row.bytes.end());
          while ((data.size() - static_cast<std::size_t>(bagOffset[b])) % kPayloadAlign != 0)
          {
            if (data.size() == u32MaxSize)
            {
              return BUILD_SIZE_OUT_OF_RANGE;
            }
            data.push_back(0);
          }
        }
        bagSize[b] = static_cast<U32>(data.size() - static_cast<std::size_t>(bagOffset[b]));
      }

      // AXES keeps slot identity and precedence rank separate.
      std::vector<unsigned char> axesPayload;
      axesPayload.push_back(static_cast<unsigned char>(axes_.size()));
      axesPayload.push_back(0);
      axesPayload.push_back(0);
      axesPayload.push_back(0);
      for (std::size_t a = 0; a < axes_.size(); ++a)
      {
        const std::size_t entryStart = axesPayload.size();
        axesPayload.push_back(static_cast<unsigned char>(axes_[a].kind));
        axesPayload.push_back(static_cast<unsigned char>(axes_[a].values.size()));
        axesPayload.push_back(rankBySlot[a]);
        axesPayload.push_back(0);
        AppendU32(axesPayload, axes_[a].baseline);
        for (std::size_t v = 0; v < axes_[a].values.size(); ++v)
        {
          AppendU16(axesPayload, axes_[a].values[v]);
        }
        while (axesPayload.size() - entryStart < kAxisEntryBytes)
        {
          axesPayload.push_back(0);
        }
      }

      const U32 dataHeaderCrc = ChunkHeaderCrc(FourCC('D', 'A', 'T', 'A'), data.size());

      // INDX. Bag CRCs cover stored bag bytes; DATA identity and extent have
      // the separate header check above so an empty bag never needs a
      // one-past vector subscript.
      std::vector<unsigned char> indexPayload;
      AppendU32(indexPayload, static_cast<U32>(bagCount_));
      AppendU32(indexPayload, static_cast<U32>(order.size()));
      for (std::size_t b = 0; b < bagCount_; ++b)
      {
        AppendU32(indexPayload, bagOffset[b]);
        AppendU32(indexPayload, bagSize[b]);
        AppendU32(indexPayload, bagSize[b]);
        Crc32 bagCrc;
        if (bagSize[b] != 0)
        {
          bagCrc.update(&data[static_cast<std::size_t>(bagOffset[b])],
                        static_cast<std::size_t>(bagSize[b]));
        }
        AppendU32(indexPayload, bagCrc.value());
        indexPayload.push_back(static_cast<unsigned char>(CODEC_NONE));
        indexPayload.push_back(0);
        indexPayload.push_back(0);
        indexPayload.push_back(0);
      }
      for (std::size_t i = 0; i < order.size(); ++i)
      {
        const Row &row = rows_[order[i]];
        AppendU32(indexPayload, row.id);
        AppendU32(indexPayload, rowOffset[order[i]]);
        AppendU32(indexPayload, static_cast<U32>(row.bytes.size()));
        indexPayload.push_back(static_cast<unsigned char>(row.bag));
        indexPayload.push_back(static_cast<unsigned char>(row.kind));
        AppendU16(indexPayload, packed[order[i]]);
      }

      const std::size_t axesChunk = kChunkHeaderBytes + AlignUp(axesPayload.size(), kPayloadAlign);
      const std::size_t indexChunk = kChunkHeaderBytes + AlignUp(indexPayload.size(), kPayloadAlign);
      const std::size_t dataChunk = kChunkHeaderBytes + AlignUp(data.size(), kPayloadAlign);
      if (!ExtentFits(u32MaxSize, kFixedHeadBytes, axesChunk) ||
          !ExtentFits(u32MaxSize - kFixedHeadBytes, axesChunk, indexChunk) ||
          !ExtentFits(u32MaxSize - kFixedHeadBytes - axesChunk, indexChunk, dataChunk))
      {
        return BUILD_SIZE_OUT_OF_RANGE;
      }
      const std::size_t totalBytes = kFixedHeadBytes + axesChunk + indexChunk + dataChunk;
      if (!SizeFitsU32(totalBytes))
      {
        return BUILD_SIZE_OUT_OF_RANGE;
      }

      const U32 axesCrc = ChunkCrc(FourCC('A', 'X', 'E', 'S'), axesPayload);
      const U32 indexCrc = ChunkCrc(FourCC('I', 'N', 'D', 'X'), indexPayload);

      built.reserve(totalBytes);
      AppendU32(built, FourCC('L', 'R', 'P', 'K'));
      AppendU32(built, static_cast<U32>(totalBytes - kFormHeaderBytes));
      AppendU32(built, FourCC('H', 'E', 'A', 'D'));
      AppendU32(built, static_cast<U32>(kHeadPayloadBytes));
      AppendU32(built, 0); // headCrc, filled after the whole fixed HEAD exists
      AppendU32(built, axesCrc);
      AppendU32(built, indexCrc);
      AppendU32(built, dataHeaderCrc);
      AppendU32(built, kFormatVersion);
      AppendU32(built, static_cast<U32>(totalBytes));
      AppendU32(built, idSpaceStamp);
      AppendU32(built, 0); // flags: V1 has no data-controlled CRC switch
      AppendU32(built, static_cast<U32>(order.size()));
      AppendU32(built, static_cast<U32>(bagCount_));
      while (built.size() < kFixedHeadBytes)
      {
        built.push_back(0);
      }

      Crc32 headCrc;
      headCrc.update(&built[kFormHeaderBytes], kChunkHeaderBytes);
      headCrc.update(&built[kHeadPayloadOffset + 4], kHeadPayloadBytes - 4);
      WriteU32BE(&built[kHeadPayloadOffset + kHeadCrc], headCrc.value());

      AppendU32(built, FourCC('A', 'X', 'E', 'S'));
      AppendU32(built, static_cast<U32>(axesPayload.size()));
      built.insert(built.end(), axesPayload.begin(), axesPayload.end());
      PadTo(built, kPayloadAlign);

      AppendU32(built, FourCC('I', 'N', 'D', 'X'));
      AppendU32(built, static_cast<U32>(indexPayload.size()));
      built.insert(built.end(), indexPayload.begin(), indexPayload.end());
      PadTo(built, kPayloadAlign);

      AppendU32(built, FourCC('D', 'A', 'T', 'A'));
      AppendU32(built, static_cast<U32>(data.size()));
      built.insert(built.end(), data.begin(), data.end());
      PadTo(built, kPayloadAlign);

      out.swap(built);
      return BUILD_OK;
    }
  } // namespace lrpc
} // namespace loka
