#include "lrpc/LrpkWriter.hpp"

#include <algorithm>

namespace loka
{
  namespace lrpc
  {
    using namespace loka::core::resource::lrpk;

    namespace
    {
      const std::size_t kFormHeaderBytes = 8;
      const std::size_t kChunkHeaderBytes = 8;
      const std::size_t kHeadPayloadBytes = kFixedHeadBytes - kFormHeaderBytes - kChunkHeaderBytes;
      const std::size_t kAxisEntryBytes = 40;
      const std::size_t kMaxBags = 16;

      struct RowLess
      {
        // Ascending id, then canonical axis order within one id. Both are
        // specified by the format so a package is reproducible.
        bool operator()(const std::pair<U32, U32> &lhs, const std::pair<U32, U32> &rhs) const
        {
          if (lhs.first != rhs.first)
          {
            return lhs.first < rhs.first;
          }
          return lhs.second < rhs.second;
        }
      };

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
    } // namespace

    Writer::Writer()
        : axes_(),
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
      row.axes = 0;
      for (std::size_t a = 0; a < axes_.size(); ++a)
      {
        const U32 index = axisValueIndex ? axisValueIndex[a] : 0;
        row.axes |= (index & 0xFUL) << (4 * a);
      }
      if (bytes && length > 0)
      {
        row.bytes.assign(bytes, bytes + length);
      }
      rows_.push_back(row);
    }

    Writer::BuildResult Writer::build(U32 idSpaceStamp, bool withCrc, std::vector<unsigned char> &out) const
    {
      out.clear();
      if (axes_.size() > kMaxAxes)
      {
        return BUILD_TOO_MANY_AXES;
      }
      if (bagCount_ > kMaxBags)
      {
        return BUILD_TOO_MANY_BAGS;
      }
      for (std::size_t a = 0; a < axes_.size(); ++a)
      {
        if (axes_[a].values.size() > kMaxAxisValues)
        {
          return BUILD_TOO_MANY_AXIS_VALUES;
        }
      }

      std::vector<std::size_t> order;
      for (std::size_t i = 0; i < rows_.size(); ++i)
      {
        for (std::size_t a = 0; a < axes_.size(); ++a)
        {
          const U32 index = (rows_[i].axes >> (4 * a)) & 0xFUL;
          if (index > axes_[a].values.size())
          {
            return BUILD_BAD_AXIS_REFERENCE;
          }
        }
        order.push_back(i);
      }

      // Selection sort by (id, axes, bag) keeps this dependency-free and the
      // row counts are small by design -- one rep normally, at most about four.
      for (std::size_t i = 0; i + 1 < order.size(); ++i)
      {
        std::size_t best = i;
        for (std::size_t j = i + 1; j < order.size(); ++j)
        {
          const Row &candidate = rows_[order[j]];
          const Row &current = rows_[order[best]];
          if (candidate.id < current.id ||
              (candidate.id == current.id &&
               (candidate.axes < current.axes || (candidate.axes == current.axes && candidate.bag < current.bag))))
          {
            best = j;
          }
        }
        const std::size_t swap = order[i];
        order[i] = order[best];
        order[best] = swap;
      }

      // Uniqueness is on (id, bag, axes): the same id legitimately appears in
      // several bags when those bags are an exclusive group such as ja/en.
      for (std::size_t i = 0; i + 1 < order.size(); ++i)
      {
        const Row &a = rows_[order[i]];
        const Row &b = rows_[order[i + 1]];
        if (a.id == b.id && a.axes == b.axes && a.bag == b.bag)
        {
          return BUILD_DUPLICATE_ROW;
        }
      }

      // The wall that makes selection total.
      for (std::size_t i = 0; i < order.size(); ++i)
      {
        const U32 id = rows_[order[i]].id;
        bool hasDefault = false;
        for (std::size_t j = 0; j < order.size(); ++j)
        {
          if (rows_[order[j]].id == id && rows_[order[j]].axes == 0)
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

      // Lay the bags out in DATA, recording each row's offset within its bag.
      std::vector<unsigned char> data;
      std::vector<U32> bagOffset(bagCount_, 0);
      std::vector<U32> bagSize(bagCount_, 0);
      std::vector<U32> rowOffset(rows_.size(), 0);
      for (std::size_t b = 0; b < bagCount_; ++b)
      {
        bagOffset[b] = static_cast<U32>(data.size());
        for (std::size_t i = 0; i < order.size(); ++i)
        {
          const Row &row = rows_[order[i]];
          if (row.bag != b)
          {
            continue;
          }
          rowOffset[order[i]] = static_cast<U32>(data.size() - bagOffset[b]);
          data.insert(data.end(), row.bytes.begin(), row.bytes.end());
          while ((data.size() - bagOffset[b]) % kPayloadAlign != 0)
          {
            data.push_back(0);
          }
        }
        bagSize[b] = static_cast<U32>(data.size() - bagOffset[b]);
      }

      // AXES.
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
        axesPayload.push_back(0);
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

      // INDX.
      std::vector<unsigned char> indexPayload;
      AppendU32(indexPayload, static_cast<U32>(bagCount_));
      AppendU32(indexPayload, static_cast<U32>(order.size()));
      for (std::size_t b = 0; b < bagCount_; ++b)
      {
        AppendU32(indexPayload, bagOffset[b]);
        AppendU32(indexPayload, bagSize[b]);
        AppendU32(indexPayload, bagSize[b]); // expanded == stored while the codec is none
        AppendU32(indexPayload,
                  withCrc ? Crc32::Of(data.empty() ? 0 : &data[bagOffset[b]], static_cast<std::size_t>(bagSize[b])) : 0);
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
        AppendU16(indexPayload, row.axes);
      }

      const std::size_t axesChunk = kChunkHeaderBytes + AlignUp(axesPayload.size(), kPayloadAlign);
      const std::size_t indexChunk = kChunkHeaderBytes + AlignUp(indexPayload.size(), kPayloadAlign);
      const std::size_t dataChunk = kChunkHeaderBytes + AlignUp(data.size(), kPayloadAlign);
      const std::size_t totalBytes = kFixedHeadBytes + axesChunk + indexChunk + dataChunk;

      out.reserve(totalBytes);
      AppendU32(out, FourCC('L', 'R', 'P', 'K'));
      AppendU32(out, static_cast<U32>(totalBytes - kFormHeaderBytes));
      AppendU32(out, FourCC('H', 'E', 'A', 'D'));
      AppendU32(out, static_cast<U32>(kHeadPayloadBytes));
      AppendU32(out, kFormatVersion);
      AppendU32(out, static_cast<U32>(totalBytes));
      AppendU32(out, idSpaceStamp);
      AppendU32(out, withCrc ? Crc32::Of(indexPayload.empty() ? 0 : &indexPayload[0], indexPayload.size()) : 0);
      AppendU32(out, withCrc ? kFlagHasCrc : 0);
      AppendU32(out, static_cast<U32>(order.size()));
      AppendU32(out, static_cast<U32>(bagCount_));
      while (out.size() < kFixedHeadBytes)
      {
        out.push_back(0);
      }

      AppendU32(out, FourCC('A', 'X', 'E', 'S'));
      AppendU32(out, static_cast<U32>(axesPayload.size()));
      out.insert(out.end(), axesPayload.begin(), axesPayload.end());
      PadTo(out, kPayloadAlign);

      AppendU32(out, FourCC('I', 'N', 'D', 'X'));
      AppendU32(out, static_cast<U32>(indexPayload.size()));
      out.insert(out.end(), indexPayload.begin(), indexPayload.end());
      PadTo(out, kPayloadAlign);

      AppendU32(out, FourCC('D', 'A', 'T', 'A'));
      AppendU32(out, static_cast<U32>(data.size()));
      out.insert(out.end(), data.begin(), data.end());
      PadTo(out, kPayloadAlign);

      return BUILD_OK;
    }
  } // namespace lrpc
} // namespace loka
