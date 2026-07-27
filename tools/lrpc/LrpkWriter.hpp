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
        /** Two rows with the same `(id, bag, axes)`. The filename-level form of
            this error is `lrpc` rejecting two files with the same axis set
            (#185 §6). */
        BUILD_DUPLICATE_ROW,
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
        BUILD_BAD_AXIS_REFERENCE
      };

      Writer();

      /** Declares one axis. Order matters: an axis's position is its nibble
          position in every row's packed `axes` field. */
      void declareAxis(core::resource::lrpk::AxisKind kind,
                       core::resource::lrpk::U32 baseline,
                       const core::resource::lrpk::U32 *values,
                       std::size_t valueCount);

      std::size_t addBag();

      /** `axisValueIndex` holds one 1-based index per declared axis, or 0 for
          "this row does not write that axis". */
      void addAsset(core::resource::lrpk::U32 id,
                    std::size_t bag,
                    core::resource::lrpk::AssetKind kind,
                    const core::resource::lrpk::U32 *axisValueIndex,
                    const unsigned char *bytes,
                    std::size_t length);

      /** Emits the package. Rows are sorted by id and then by canonical axis
          order, both specified rather than left to the implementation
          (#185 §14). */
      BuildResult build(core::resource::lrpk::U32 idSpaceStamp, bool withCrc, std::vector<unsigned char> &out) const;

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
              axes(0),
              bytes()
        {
        }

        core::resource::lrpk::U32 id;
        std::size_t bag;
        core::resource::lrpk::AssetKind kind;
        core::resource::lrpk::U32 axes;
        std::vector<unsigned char> bytes;
      };

      std::vector<Axis> axes_;
      std::vector<Row> rows_;
      std::size_t bagCount_;
    };
  } // namespace lrpc
} // namespace loka

#endif // LOKA_TOOLS_LRPC_LRPKWRITER_HPP
