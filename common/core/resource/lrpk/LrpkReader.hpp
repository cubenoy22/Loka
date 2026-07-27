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
        /** What the destination context can answer about itself. One entry per
            declared axis.
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
          /** Enum axis: the declared vocabulary value; the reader maps it to
              the package's physical slot. Scalar axis: the measured number
              itself, for example a scale percentage. */
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

            **The reader borrows; it never owns.** `openBorrowedBytes` keeps the
            caller's pointer and hands out pointers into it, because §7's whole
            point is that a loaded bag is served without a copy -- a 4 MB target
            cannot afford the alternative. That makes the buffer's lifetime part
            of the contract rather than an implementation detail: the bytes
            passed to `openBorrowedBytes` must outlive the reader and everything
            it returned. The name says "borrowed" so no call site has to infer
            it.

            The file-backed entry point, which seeks once to a bag head and
            reads forward, is stage 2 of the vertical slice and needs the
            locator from #199. */
        class Reader
        {
        public:
          enum IntegrityMode
          {
            VERIFY_INTEGRITY = 0,
            SKIP_INTEGRITY
          };

          enum OpenResult
          {
            OPEN_OK = 0,
            /** The form header is not "LRPK", or the buffer is too short to
                hold the fixed head. */
            OPEN_NOT_A_PACKAGE,
            OPEN_UNSUPPORTED_VERSION,
            /** The borrowed host buffer cannot be represented by the
                format's 32-bit total-length field. */
            OPEN_SIZE_OUT_OF_RANGE,
            /** The size recorded in HEAD disagrees with the buffer -- the
                cheapest and most effective check there is (#185 §10). */
            OPEN_TRUNCATED,
            OPEN_HEAD_CORRUPT,
            OPEN_INDEX_CORRUPT,
            /** V1 knows the exact required and optional chunk set. Case does
                not demote an unknown chunk to ignorable. */
            OPEN_UNKNOWN_CHUNK,
            /** The package was built against a different id space than the
                header the application compiled against. Never waived, not even
                when integrity verification is skipped: it happens daily and
                its symptom is silent
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
            /** Selection did not end with exactly one row. Well-formed
                packages cannot reach this: `lrpc` requires an axis-free
                default and refuses same-bag ambiguity, while `openBag()`
                refuses cross-bag overlap. Kept because a reader may not
                assume its input is well formed. */
            GET_NO_MATCHING_REP
          };

          enum BagResult
          {
            BAG_OK = 0,
            BAG_NO_SUCH_BAG,
            BAG_CONTENTS_CORRUPT,
            BAG_UNSUPPORTED_CODEC,
            /** This bag shares an asset id with a bag already open. Selection
                never chooses between bags, so the second open is refused. */
            BAG_ASSET_ID_CONFLICT
          };

          Reader();

          /** Validates the head, the version-required chunk set, and every
              structural invariant. CRC verification follows `integrityMode`;
              format validation and id-space identity never do. Nothing is
              decoded and no bag is loaded.

              Failure-atomic: a reader already holding a good package keeps it,
              open bags and all, if a reload is refused. Parsing happens into a
              separate state which is committed only on `OPEN_OK`.

              @param bytes The whole package. **Borrowed** -- it must outlive
                           this reader and every `Asset` obtained from it.
              @param integrityMode Whether CRC values are verified. The
                                   package always carries them and cannot
                                   disable its own inspection. */
          OpenResult openBorrowedBytes(const unsigned char *bytes,
                                       std::size_t size,
                                       U32 expectedIdSpaceStamp,
                                       IntegrityMode integrityMode);

          void close();

          bool isOpen() const { return state_.bytes != 0; }
          bool verifiesIntegrity() const { return state_.verifyIntegrity; }
          std::size_t bagCount() const { return state_.bagCount; }
          std::size_t assetCount() const { return state_.assetCount; }
          U32 idSpaceStamp() const { return state_.idSpaceStamp; }

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
          bool rowIsEligible(const unsigned char *row, const Facts &facts) const;
          bool rowMatchesFilters(const unsigned char *row,
                                 const bool *filtered,
                                 const bool *enumRequiresWritten,
                                 const U32 *scalarValues) const;
          U32 rowScalarValue(const unsigned char *row, std::size_t axis) const;
          static U32 RowAxisIndex(const unsigned char *row, std::size_t axis);

          /** Everything parsing produces. Held as one value so a refused
              reload cannot leave the reader half-updated: `openBorrowedBytes`
              fills a local State and assigns it only after every check passes. */
          struct State
          {
            State()
                : bytes(0),
                  size(0),
                  verifyIntegrity(false),
                  idSpaceStamp(0),
                  dataPayload(0),
                  dataPayloadSize(0),
                  assetRows(0),
                  bagCount(0),
                  assetCount(0),
                  axisCount(0)
            {
              for (std::size_t i = 0; i < kMaxAxes; ++i)
              {
                precedenceSlots[i] = 0;
              }
            }

            const unsigned char *bytes;
            std::size_t size;
            bool verifyIntegrity;
            U32 idSpaceStamp;
            const unsigned char *dataPayload;
            std::size_t dataPayloadSize;
            const unsigned char *assetRows;
            std::size_t bagCount;
            std::size_t assetCount;
            std::size_t axisCount;
            unsigned char precedenceSlots[kMaxAxes];
            Axis axes[kMaxAxes];
            Bag bags[kMaxBags];
          };

          State state_;
        };
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_RESOURCE_LRPK_LRPKREADER_HPP
