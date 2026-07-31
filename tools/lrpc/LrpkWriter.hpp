#ifndef LOKA_TOOLS_LRPC_LRPKWRITER_HPP
#define LOKA_TOOLS_LRPC_LRPKWRITER_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "core/resource/lrpk/LrpkFormat.hpp"

namespace loka
{
  namespace lrpc
  {
    /** The source-side facts that decide one row's physical position in DATA.

        A declaration-owned order is stronger than the source path. Rows not
        named by such a declaration fall back to bytewise lexicographic path
        order, so package bytes do not depend on host locale or encoding. */
    class AssetLayoutKey
    {
    public:
      explicit AssetLayoutKey(const std::string &sourcePath)
          : hasDeclaredOrder_(false),
            declaredOrder_(0),
            path_(sourcePath)
      {
      }

      AssetLayoutKey(std::size_t order, const std::string &sourcePath)
          : hasDeclaredOrder_(true),
            declaredOrder_(order),
            path_(sourcePath)
      {
      }

      bool hasDeclaredOrder() const { return hasDeclaredOrder_; }
      std::size_t declaredOrder() const { return declaredOrder_; }
      const std::string &path() const { return path_; }

    private:
      bool hasDeclaredOrder_;
      std::size_t declaredOrder_;
      std::string path_;
    };

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
        /** A declared selector index exceeds that axis's value count. Slots
            beyond the declared axis count cannot enter through `addAsset()`. */
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
        BUILD_NULL_PAYLOAD,
        /** An axis declaration followed the first asset row. Axis count is part
            of `addAsset()`'s selector-width contract and is immutable once rows
            exist. */
        BUILD_AXIS_AFTER_ASSET
      };

      Writer();

      /** Declares one axis. Order matters: an axis's position is its nibble
          position in every row's packed `axes` field. All axes must be declared
          before the first `addAsset()`; a later declaration is recorded and
          reported by `build()` as `BUILD_AXIS_AFTER_ASSET`. */
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

      /** `layoutKey` controls only physical DATA placement; the index remains
          sorted by id, axes and bag. When non-null, `axisValueIndex` holds
          exactly `axes_.size()` entries at
          call time: one 1-based index per declared axis, or 0 for "this row
          does not write that axis". Null writes no axis. `bytes` may be null
          only when `length` is zero. */
      void addAsset(const AssetLayoutKey &layoutKey,
                    core::resource::lrpk::U32 id,
                    std::size_t bag,
                    core::resource::lrpk::AssetKind kind,
                    const core::resource::lrpk::U32 *axisValueIndex,
                    const unsigned char *bytes,
                    std::size_t length);

      /** Emits the package. Index rows are sorted by id and then by canonical
          axis order (#185 §14); DATA follows each row's layout key (#185
          §7). Both are specified rather than left to the implementation. */
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
            : layoutKey(""),
              id(0),
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

        AssetLayoutKey layoutKey;
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
      BuildResult constructionError_;
    };
  } // namespace lrpc
} // namespace loka

#endif // LOKA_TOOLS_LRPC_LRPKWRITER_HPP
