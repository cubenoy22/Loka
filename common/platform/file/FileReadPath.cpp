// Toolbox defines its own ReadBytes(path) around the same stdio read
// (apple/toolbox/src/platform/ToolboxFileIO.cpp, #1066).
#if !defined(LOKA_RETRO68)

#include "platform/file/FileIO.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      ReadResult ReadBytes(const loka::core::String &path, std::vector<unsigned char> &out,
                           const ReadCapacity *capacity)
      {
        return ReadBytesThroughStdio(path, out, capacity);
      }
    } // namespace file
  } // namespace platform
} // namespace loka

#endif
