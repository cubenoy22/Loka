#ifndef LOKA_TOOLBOX_BYTE_SOURCE_HPP
#define LOKA_TOOLBOX_BYTE_SOURCE_HPP

#include <cstddef>
#include <Files.h>

#include "core/resource/lrpk/LrpkReader.hpp"

namespace loka
{
  namespace toolbox
  {
    /** A `ByteSource` over the Toolbox File Manager, for the reader running on
        the machine an `.lrpk` actually ships to. `StdioByteSource` exercises
        the same file-backed reader on development hosts; this is the other
        implementation of the same narrow interface, opened against an
        `FSSpec` rather than a path string because that is what Classic hands
        callers (a chosen file, an application-relative lookup) once they
        have resolved one.

        Non-copyable, because a File Manager reference number has one owner
        and the file mark is shared state; two handles to the same reference
        number would seek out from under each other. */
    class ToolboxByteSource : public loka::core::resource::lrpk::ByteSource
    {
    public:
      ToolboxByteSource();
      virtual ~ToolboxByteSource();

      /** Opens for reading via `FSpOpenDF`. Any previously open file is
          closed first, so reopening cannot leak the old reference number.
          False leaves the source closed. */
      bool open(const FSSpec &spec);
      void close();
      bool isOpen() const { return open_; }

      virtual bool readAt(std::size_t at, unsigned char *dst, std::size_t n);
      virtual bool size(std::size_t &out);

    private:
      ToolboxByteSource(const ToolboxByteSource &);
      ToolboxByteSource &operator=(const ToolboxByteSource &);

      short refNum_;
      bool open_;
    };
  } // namespace toolbox
} // namespace loka

#endif // LOKA_TOOLBOX_BYTE_SOURCE_HPP
