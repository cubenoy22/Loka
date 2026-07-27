#ifndef LOKA_CORE_RESOURCE_LRPK_LRPKREADER_HPP
#define LOKA_CORE_RESOURCE_LRPK_LRPKREADER_HPP

#include <cstddef>

#include "core/resource/lrpk/LrpkFormat.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      namespace lrpk
      {
        /** What the destination context can answer about itself, in the form
            the row encoding uses. One entry per declared axis.
            `present[a] == false` means the target cannot answer that axis at
            all, which is not the same as any value it could hold — a row that
            writes an axis the target cannot answer is dropped rather than
            matched (#189's absent rule, #185 §14). */
        struct Facts
        {
          Facts()
          {
            for (std::size_t i = 0; i < kMaxAxes; ++i)
            {
              present[i] = false;
              value[i] = 0;
            }
          }

          bool present[kMaxAxes];
          /** Enum axis: the declared value index, 1..15. Scalar axis: the
              measured number itself, for example a scale percentage. */
          U32 value[kMaxAxes];
        };

        /** One selected representation. `bytes` points into the loaded bag, so
            it is valid until that bag is closed — closing a bag invalidates
            everything handed out of it (#185 §14). */
        struct Asset
        {
          Asset()
              : bytes(0),
                length(0),
                kind(ASSET_KIND_UNKNOWN)
          {
          }

          const unsigned char *bytes;
          std::size_t length;
          AssetKind kind;
        };

        /** Reader for a compiled package. A reader, never a resolver: it opens,
            validates, loads a bag, and serves `get(id, facts)` from memory
            (#185 §1, §7).

            This overload takes the whole package as bytes. The file-backed
            entry point, which seeks once to a bag head and reads forward, is
            stage 2 of the vertical slice and needs the locator from #199. */
        class Reader
        {
        public:
          enum OpenResult
          {
            OPEN_OK = 0,
            /** The form header is not "LRPK", or the buffer is too short to
                hold the fixed head. */
            OPEN_NOT_A_PACKAGE,
            OPEN_UNSUPPORTED_VERSION,
            /** The size recorded in HEAD disagrees with the buffer -- the
                cheapest and most effective check there is (#185 §10). */
            OPEN_TRUNCATED,
            OPEN_INDEX_CORRUPT,
            /** An unknown chunk whose 4CC begins with an uppercase letter.
                Skipping it unconditionally would let a required chunk go
                silently missing (#185 §14). */
            OPEN_UNKNOWN_CRITICAL_CHUNK,
            /** The package was built against a different id space than the
                header the application compiled against. Never waived, not even
                in unsafe mode: it happens daily and its symptom is silent
                (#185 §10). */
            OPEN_ID_SPACE_MISMATCH,
            OPEN_MALFORMED_INDEX
          };

          enum GetResult
          {
            GET_OK = 0,
            GET_NO_SUCH_ID,
            /** The application has not opened the bag this id lives in. This is
                not asset absence -- that is impossible by construction -- it is
                a different failure and must not be shown as the same one
                (#185 §14). */
            GET_BAG_NOT_OPEN,
            /** No row survived selection. Well-formed packages cannot reach
                this: `lrpc` fails the build for an asset with no axis-free
                default row. Kept because a reader may not assume its input is
                well formed. */
            GET_NO_MATCHING_REP
          };

          enum BagResult
          {
            BAG_OK = 0,
            BAG_NO_SUCH_BAG,
            BAG_CONTENTS_CORRUPT,
            BAG_UNSUPPORTED_CODEC
          };

          Reader();

          /** Validates the head, the index and every critical chunk. Nothing is
              decoded and no bag is loaded. */
          OpenResult openFromMemory(const unsigned char *bytes, std::size_t size, U32 expectedIdSpaceStamp);

          void close();

          bool isOpen() const { return bytes_ != 0; }
          bool hasCrc() const { return (flags_ & kFlagHasCrc) != 0; }
          std::size_t bagCount() const { return bagCount_; }
          std::size_t assetCount() const { return assetCount_; }
          U32 idSpaceStamp() const { return idSpaceStamp_; }

          /** Verifies the bag against its recorded CRC and makes its rows
              addressable. Memory-backed, so this costs one pass over the bag
              and no copy; the file-backed reader accumulates the same CRC while
              reading, for zero extra I/O (#185 §10). */
          BagResult openBag(std::size_t bagIndex);
          void closeBag(std::size_t bagIndex);
          bool isBagOpen(std::size_t bagIndex) const;

          GetResult get(U32 id, const Facts &facts, Asset &out) const;

        private:
          struct Bag
          {
            Bag()
                : dataOffset(0),
                  storedSize(0),
                  expandedSize(0),
                  crc(0),
                  codec(CODEC_NONE),
                  open(false)
            {
            }

            U32 dataOffset;
            U32 storedSize;
            U32 expandedSize;
            U32 crc;
            unsigned char codec;
            bool open;
          };

          struct Axis
          {
            Axis()
                : kind(AXIS_KIND_ENUM),
                  valueCount(0),
                  baseline(0)
            {
              for (std::size_t i = 0; i < kMaxAxisValues; ++i)
              {
                values[i] = 0;
              }
            }

            unsigned char kind;
            unsigned char valueCount;
            U32 baseline;
            U32 values[kMaxAxisValues];
          };

          /** Returns the index of the first row with this id, or assetCount_ if
              absent. Rows are sorted by id, so this is a binary search; ids are
              sparse inside a package because numbering runs over the whole tree
              (#185 §14). */
          std::size_t findFirstRow(U32 id) const;
          const unsigned char *assetRow(std::size_t index) const;
          bool rowSurvivesEnumAxes(const unsigned char *row, const Facts &facts) const;
          U32 rowScalarValue(const unsigned char *row, std::size_t axis, bool &written) const;
          static U32 RowAxisIndex(const unsigned char *row, std::size_t axis);
          static std::size_t RowWrittenAxisCount(const unsigned char *row);

          const unsigned char *bytes_;
          std::size_t size_;
          U32 flags_;
          U32 idSpaceStamp_;

          const unsigned char *dataPayload_;
          std::size_t dataPayloadSize_;

          const unsigned char *bagRows_;
          const unsigned char *assetRows_;
          std::size_t bagCount_;
          std::size_t assetCount_;

          std::size_t axisCount_;
          Axis axes_[kMaxAxes];
          Bag bags_[16];
        };
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_RESOURCE_LRPK_LRPKREADER_HPP
