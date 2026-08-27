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
        declaration file rather than an early draft of it. V1 deliberately
        keeps this explicit manifest as the CLI input: filesystem discovery
        cannot define the deferred "one file, many string ids" shape yet.
        A later discovery front half must produce records of this shape; it
        does not replace them. */
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
          vertical slice; generated `R.hpp` reads this source-side name. */
      std::string name;
      std::string source;
    };

    struct PackManifest
    {
      std::vector<std::string> bags;
      std::vector<ManifestAsset> assets;
    };

    struct RequirementViolation
    {
      RequirementViolation()
          : line(0), message()
      {
      }

      std::size_t line;
      std::string message;
    };

    enum RequirementResult
    {
      REQUIREMENTS_OK = 0,
      REQUIREMENTS_CANNOT_READ,
      REQUIREMENTS_UNKNOWN_DIRECTIVE,
      REQUIREMENTS_BAD_FIELD_COUNT,
      REQUIREMENTS_BAD_INDEX,
      REQUIREMENTS_BAD_ID,
      REQUIREMENTS_BAD_KIND,
      REQUIREMENTS_BAD_ASSET_FORM,
      REQUIREMENTS_BAD_PAGES_FORM,
      REQUIREMENTS_PAGE_COUNT_REQUIRED,
      REQUIREMENTS_EMBEDDED_NUL,
      REQUIREMENTS_EMPTY
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
      /** Two rows share a symbolic name. The name-to-id association has to be
          a function for a header to be generated from it, and the stamp hashes
          that association, so an ambiguous name is refused rather than packed
          into something no `R.hpp` could express. */
      MANIFEST_DUPLICATE_NAME,
      MANIFEST_DUPLICATE_BAG,
      /** A NUL byte anywhere in the manifest. A text manifest has no use for
          one, and carrying it into a field is not harmless: a path holding a
          NUL compares as its full length everywhere in this tool while
          `fopen` sees only the part before it, so a source spelled
          `payload\0suffix` is read from `payload` while every collision check
          believes a different file was named. */
      MANIFEST_EMBEDDED_NUL,
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

    /** Checks the three supported structural requirements in source order:

            bag <index> <name>
            asset <id> <image|string|audio> in <bag-name>
            pages <first-id> count-from bag <first-bag> kinds <kind-list>

        `kind-list` is comma-separated. `requiredPageCount` supplies the
        producer-owned count for every `pages` line; omitting it when one is
        present is a hard error. That count must equal the number of bags
        beginning at `first-bag`, and each id must occupy its corresponding
        bag with an allowed kind. The package is closed to unlisted assets:
        its total asset count must equal the page count plus the number of
        `asset` lines. Every valid but violated rule is appended to
        `violations`; malformed input is a hard error. */
    RequirementResult CheckPackageRequirements(
        const char *text,
        std::size_t length,
        const PackManifest &manifest,
        const std::size_t *requiredPageCount,
        std::vector<RequirementViolation> &violations,
        std::size_t &errorLine);

    /** Reads and checks a requirements file without flattening Win32 paths. */
    RequirementResult CheckPackageRequirementsFile(
        const char *path,
        const PackManifest &manifest,
        const std::size_t *requiredPageCount,
        std::vector<RequirementViolation> &violations,
        std::size_t &errorLine);
#if defined(_WIN32)
    RequirementResult CheckPackageRequirementsFile(
        const wchar_t *path,
        const PackManifest &manifest,
        const std::size_t *requiredPageCount,
        std::vector<RequirementViolation> &violations,
        std::size_t &errorLine);
#endif

    /** Derives the id-space stamp from the generated baked-resource facts.

        #185 §10 gives the stamp one job: detecting a package that is out of
        step with the header the application compiled against, a failure that
        "happens daily and whose symptom is silent". A stamp typed by hand into
        both the manifest and the application header cannot do that job -- it
        only catches a literal mismatch, so renumbering an asset leaves the
        stamp agreeing while every lookup has moved.

        It covers the **generated baked-resource facts**, as the bag count plus
        sorted `(id, kind, bag, name)` rows, not just the set of ids. Bag
        membership is included because generated `AssetRef` values carry it;
        inserting or moving a bag must not leave an older header accepted.
        Hashing `(id, kind)` alone would let two
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
