#ifndef LOKA_TOOLS_LRPC_HOSTFILE_HPP
#define LOKA_TOOLS_LRPC_HOSTFILE_HPP

#include <string>
#include <vector>

namespace loka
{
  namespace lrpc
  {
    /** Reads one host-build input without assigning an encoding to its bytes. */
    bool ReadWholeFile(const std::string &path, std::vector<unsigned char> &out);

#if defined(_WIN32)
    /** Wide-path form used at the Win32 filesystem boundary. */
    bool ReadWholeFile(const std::wstring &path, std::vector<unsigned char> &out);
#endif
  } // namespace lrpc
} // namespace loka

#endif // LOKA_TOOLS_LRPC_HOSTFILE_HPP
