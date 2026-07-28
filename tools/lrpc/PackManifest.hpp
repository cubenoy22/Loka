#ifndef LOKA_TOOLS_LRPC_PACKMANIFEST_HPP
#define LOKA_TOOLS_LRPC_PACKMANIFEST_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "core/resource/lrpk/LrpkFormat.hpp"

namespace loka
{
  namespace lrpc
  {
    /** One canonical, package-ready asset record.

        #206's accepted plan amendment fixes what the pack stage is handed: a
        stable symbolic name, an assigned numeric id, an asset kind, a bag
        assignment and a completed byte payload. The manifest is that contract
        written down, nothing more -- it is deliberately *below* #185 §6's
        declaration file rather than an early draft of it. When the string
        shape rally lands and `lrpc` grows a front half that walks `Assets/`,
        that front half produces records of this shape; it does not replace
        them. */
    struct ManifestAsset
    {
      ManifestAsset()
          : id(0),
            kind(core::resource::lrpk::ASSET_KIND_UNKNOWN),
            bag(0),
            name(),
            source()
      {
      }

      core::resource::lrpk::U32 id;
      core::resource::lrpk::AssetKind kind;
      std::size_t bag;
      /** Carried but not written to the package. The id->name table is the
          `"nams"` chunk, which is a diagnostic-build concern and out of the
          vertical slice; the column exists now because the pack-input
          contract names it and because `R.hpp` generation will read it. */
      std::string name;
      std::string source;
    };

    struct PackManifest
    {
      std::vector<std::string> bags;
      std::vector<ManifestAsset> assets;
    };

    enum ManifestResult
    {
      MANIFEST_OK = 0,
      MANIFEST_UNKNOWN_DIRECTIVE,
      MANIFEST_BAD_FIELD_COUNT,
      MANIFEST_BAD_ID,
      MANIFEST_BAD_KIND,
      /** An `asset` line before any `bag` line. Assets join the most recently
          declared bag, so the first bag has to exist. */
      MANIFEST_ASSET_BEFORE_BAG,
      MANIFEST_DUPLICATE_ID,
      MANIFEST_DUPLICATE_BAG,
      MANIFEST_EMPTY
    };

    /** Parses the manifest text. Pure: no file is opened here, so the grammar
        and its refusals are pinnable headlessly and the tool's I/O stays in
        one place.

        Grammar, one directive per line, `#` to end of line is a comment:

            bag   <name>
            asset <id> <image|string|audio> <name> <source>

        An `asset` joins the most recently declared bag. `bags` are indexed in
        declaration order, which is the index `Writer::addBag()` returns.

        @param errorLine 1-based line of the refusal, or 0 when the whole file
                         is at fault. */
    ManifestResult ParseManifest(const char *text,
                                 std::size_t length,
                                 PackManifest &out,
                                 std::size_t &errorLine);

    /** Derives the id-space stamp from the id assignment itself.

        #185 §10 gives the stamp one job: detecting a package that is out of
        step with the header the application compiled against, a failure that
        "happens daily and whose symptom is silent". A stamp typed by hand into
        both the manifest and the application header cannot do that job -- it
        only catches a literal mismatch, so renumbering an asset leaves the
        stamp agreeing while every lookup has moved.

        It covers the **symbol-to-id association**, as sorted `(id, kind, name)`
        rows, not just the set of ids. Hashing `(id, kind)` alone would let two
        same-kind symbols exchange ids without moving the stamp -- the row
        multiset is identical while the header now points each symbol at the
        other's bytes, which is exactly the silent mismatch being guarded
        against.

        Source paths are excluded: they are build-side bookkeeping and never
        reach the application, so renaming a file on disk must not invalidate
        every header. Hashing a name is not the deferred string-shape rally,
        which is about how `lrpc` *discovers* names in its input. */
    core::resource::lrpk::U32 DeriveIdSpaceStamp(const PackManifest &manifest);
  } // namespace lrpc
} // namespace loka

#endif // LOKA_TOOLS_LRPC_PACKMANIFEST_HPP
