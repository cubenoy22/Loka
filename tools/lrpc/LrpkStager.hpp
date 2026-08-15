#ifndef LOKA_LRPC_LRPKSTAGER_HPP
#define LOKA_LRPC_LRPKSTAGER_HPP

#include <cstddef>
#include <vector>

namespace loka
{
  namespace lrpc
  {
    enum StagePackageResult
    {
      STAGE_PACKAGE_OK = 0,
      STAGE_PACKAGE_NOT_FIXED_HEAD,
      STAGE_PACKAGE_FORM_LENGTH_MISMATCH,
      STAGE_PACKAGE_TRUNCATED_CHUNK_HEADER,
      STAGE_PACKAGE_TRUNCATED_CHUNK_PAYLOAD,
      STAGE_PACKAGE_MISSING_INDEX_OR_DATA,
      STAGE_PACKAGE_INDEX_TOO_SHORT,
      STAGE_PACKAGE_INDEX_ROW_COUNTS_MISMATCH,
      STAGE_PACKAGE_BAG_OUT_OF_RANGE,
      STAGE_PACKAGE_INVALID_BAG_PAYLOAD_BOUNDS,
      STAGE_PACKAGE_CORRUPTION_BYTE_OUT_OF_BOUNDS
    };

    struct CorruptionSite
    {
      CorruptionSite();

      std::size_t payloadStart;
      std::size_t payloadEnd;
      std::size_t byteOffset;
    };

    /** Validates an LRPK package and produces a staged byte-for-byte copy.
        When `corruptBag` is non-null, one byte in that bag's stored payload is
        flipped after every structural bound has been validated. A null bag
        still validates bag zero, preserving the scenario runner's copy-only
        staging contract. */
    StagePackageResult StagePackageBytes(
        const std::vector<unsigned char> &source,
        const std::size_t *corruptBag,
        std::vector<unsigned char> &staged,
        CorruptionSite &site);
  } // namespace lrpc
} // namespace loka

#endif // LOKA_LRPC_LRPKSTAGER_HPP
