#ifndef LOKA_CORE_RESOURCE_LRPK_LRPKSTDIOBYTESOURCE_HPP
#define LOKA_CORE_RESOURCE_LRPK_LRPKSTDIOBYTESOURCE_HPP

#include <cstddef>
#include <cstdio>

#include "core/String.hpp"
#include "core/resource/lrpk/LrpkReader.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      namespace lrpk
      {
        /** A `ByteSource` over stdio, for hosts that have it -- development
            machines and CI. It exists so the file-backed reader can be
            exercised everywhere the tests run; the Toolbox source that Classic
            actually ships with is a separate implementation of the same
            interface, which is the point of the interface being this narrow.

            Non-copyable, because a `FILE*` has one owner and the file position
            is shared state; two handles to the same stream would seek out from
            under each other. */
        class StdioByteSource : public ByteSource
        {
        public:
          StdioByteSource();
          virtual ~StdioByteSource();

          /** Opens for reading through the platform file seam, which
              materializes the logical String in the platform's native path
              form. A raw `fopen` would decode a Win32 path in the process
              ANSI code page and lose full-width characters (#15); Classic
              deliberately keeps `fopen` behind the same seam. Any previously
              open file is closed first, so reopening cannot leak the old
              handle. False leaves the source closed. */
          bool open(const loka::core::String &path);
          void close();
          bool isOpen() const { return file_ != 0; }

          virtual bool readAt(std::size_t at, unsigned char *dst, std::size_t n);
          virtual bool size(std::size_t &out);

        private:
          StdioByteSource(const StdioByteSource &);
          StdioByteSource &operator=(const StdioByteSource &);

          std::FILE *file_;
        };
      } // namespace lrpk
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_RESOURCE_LRPK_LRPKSTDIOBYTESOURCE_HPP
