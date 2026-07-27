#ifndef LOKA_TOOLS_LRPC_LRPKWRITER_HPP
#define LOKA_TOOLS_LRPC_LRPKWRITER_HPP

#include <cstddef>
#include <vector>

#include "core/resource/lrpk/LrpkFormat.hpp"

namespace loka
{
  namespace lrpc
  {
    /** Build-time writer for a package. Deliberately not in `common/`: the
        runtime is a reader and nothing an application links should be able to
        produce a package (#185 §1).

        This is the pack stage only. Compiling an asset into its target format
        is a separate, independently cacheable stage (#185 §13) and is out of
        scope for the vertical slice (#206), so the bytes handed in here are
        already in the form the target consumes. */
    class Writer
    {
    public:
      enum BuildResult
      {
        BUILD_OK = 0,
        /** An asset with no axis-free row. This is the wall that makes
            selection total: the default row survives every enum rule and is
            picked up by the scalar rule's "if none is at or above, the
            largest", so `get()` can never come up empty (#185 §14). */
        BUILD_ASSET_WITHOUT_DEFAULT_ROW,
        /** Two rows for one id disagree on `AssetKind`. */
        BUILD_ASSET_KIND_MISMATCH,
        /** A row names a bag index that `addBag()` never returned. */
        BUILD_BAD_BAG_REFERENCE,
        BUILD_TOO_MANY_AXES,
        BUILD_TOO_MANY_AXIS_VALUES,
        BUILD_TOO_MANY_BAGS,
        BUILD_BAD_AXIS_REFERENCE,
        BUILD_BAD_AXIS_KIND,
        /** A row carries an `AssetKind` outside the format's closed set. */
        BUILD_BAD_ASSET_KIND,
        BUILD_BAD_AXIS_VOCABULARY,
        /** The package policy is not an exact permutation of every declared
            axis. Required for two or more axes; normalized for zero or one. */
        BUILD_BAD_PRECEDENCE,
        /** Two rows cannot be distinguished by the selector after all phases.
            Physical row order is never a winner rule. This build-time wall is
            same-bag only; the reader completes it across bags by refusing an
            overlapping second `openBag()` as `BAG_ASSET_ID_CONFLICT`. */
        BUILD_SELECTOR_AMBIGUOUS,
        /** A declared axis value does not fit the 16-bit encoded field.
            Silently truncating would let 65536 become 0 and select the
            wrong representation from a package that built cleanly. */
        BUILD_AXIS_VALUE_OUT_OF_RANGE,
        /** A host-side size or U32 API value cannot be represented in a
            32-bit on-disk field. */
        BUILD_SIZE_OUT_OF_RANGE,
        /** A non-empty asset was supplied without payload bytes. */
        BUILD_NULL_PAYLOAD
      };

      Writer();

      /** Declares one axis. Order matters: an axis's position is its nibble
          position in every row's packed `axes` field. */
      void declareAxis(core::resource::lrpk::AxisKind kind,
                       core::resource::lrpk::U32 baseline,
                       const core::resource::lrpk::U32 *values,
                       std::size_t valueCount);

      /** Declares package-owned representation precedence as axis slots in
          highest-to-lowest order. Slot identity remains the declaration
          position used by row nibbles; changing this order does not rewrite
          those nibbles. For zero or one axis this may be omitted. */
      void setRepresentationPrecedence(const std::size_t *axisSlots, std::size_t axisCount);

      std::size_t addBag();

      /** `axisValueIndex` holds one 1-based index per declared axis, or 0 for
          "this row does not write that axis". `bytes` may be null only when
          `length` is zero. */
      void addAsset(core::resource::lrpk::U32 id,
                    std::size_t bag,
                    core::resource::lrpk::AssetKind kind,
                    const core::resource::lrpk::U32 *axisValueIndex,
                    const unsigned char *bytes,
                    std::size_t length);

      /** Emits the package. Rows are sorted by id and then by canonical axis
          order, both specified rather than left to the implementation
          (#185 §14). */
      BuildResult build(core::resource::lrpk::U32 idSpaceStamp, std::vector<unsigned char> &out) const;

    private:
      struct Axis
      {
        Axis()
            : kind(core::resource::lrpk::AXIS_KIND_ENUM),
              baseline(0),
              values()
        {
        }

        core::resource::lrpk::AxisKind kind;
        core::resource::lrpk::U32 baseline;
        std::vector<core::resource::lrpk::U32> values;
      };

      struct Row
      {
        Row()
            : id(0),
              bag(0),
              kind(core::resource::lrpk::ASSET_KIND_UNKNOWN),
              nullPayload(false),
              bytes()
        {
          for (std::size_t i = 0; i < core::resource::lrpk::kMaxAxes; ++i)
          {
            axisIndex[i] = 0;
          }
        }

        core::resource::lrpk::U32 id;
        std::size_t bag;
        core::resource::lrpk::AssetKind kind;
        /** Preserves the invalid null/non-empty input until build reports it. */
        bool nullPayload;
        /** Raw, unmasked indices as the caller supplied them. Packing into
            the 16-bit field happens in build(), after validation: masking on
            the way in turned an out-of-range 16 into 0, which reads as "this
            row writes no axis" and would be mistaken for the default row. */
        core::resource::lrpk::U32 axisIndex[core::resource::lrpk::kMaxAxes];
        std::vector<unsigned char> bytes;
      };

      std::vector<Axis> axes_;
      std::vector<std::size_t> precedence_;
      std::vector<Row> rows_;
      std::size_t bagCount_;
    };
  } // namespace lrpc
} // namespace loka

#endif // LOKA_TOOLS_LRPC_LRPKWRITER_HPP
