#ifndef LOKA_TOOLS_LRPC_UTF8PATH_HPP
#define LOKA_TOOLS_LRPC_UTF8PATH_HPP

#include <string>

namespace loka
{
  namespace lrpc
  {
    /** Converts a manifest-owned UTF-8 path to the host's wide string shape.
        Invalid UTF-8 is refused and leaves `out` unchanged. */
    bool Utf8PathToWide(const std::string &utf8, std::wstring &out);
  } // namespace lrpc
} // namespace loka

#endif // LOKA_TOOLS_LRPC_UTF8PATH_HPP
