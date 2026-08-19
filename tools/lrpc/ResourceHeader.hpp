#ifndef LOKA_TOOLS_LRPC_RESOURCEHEADER_HPP
#define LOKA_TOOLS_LRPC_RESOURCEHEADER_HPP

#include <string>

#include "lrpc/PackManifest.hpp"

namespace loka
{
  namespace lrpc
  {
    enum ResourceHeaderResult
    {
      RESOURCE_HEADER_OK = 0,
      RESOURCE_HEADER_BAD_SYMBOL,
      RESOURCE_HEADER_NEEDS_NAMESPACE,
      RESOURCE_HEADER_SYMBOL_COLLISION,
      RESOURCE_HEADER_RESERVED_SYMBOL
    };

    /** Generates the C++98 baked-resource surface from one completed pack
        manifest. The header and package therefore describe the same immutable
        id, bag, kind, and id-space-stamp facts.

        Symbol names use `/` as a namespace boundary and must contain at least
        one namespace plus the value name. Every segment must be a portable C++
        identifier across the supported language modes; invalid, reserved, and
        prefix-colliding names are refused rather than rewritten into an
        ambiguous API.

        `out` is replaced only on success. */
    ResourceHeaderResult GenerateResourceHeader(const PackManifest &manifest, std::string &out);
  } // namespace lrpc
} // namespace loka

#endif // LOKA_TOOLS_LRPC_RESOURCEHEADER_HPP
