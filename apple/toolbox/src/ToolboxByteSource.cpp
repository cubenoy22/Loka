#include "ToolboxByteSource.hpp"

#include <climits>

namespace loka
{
  namespace toolbox
  {
    ToolboxByteSource::ToolboxByteSource()
        : refNum_(0),
          open_(false)
    {
    }

    ToolboxByteSource::~ToolboxByteSource()
    {
      close();
    }

    bool ToolboxByteSource::open(const FSSpec &spec)
    {
      close();
      short refNum = 0;
      // fsRdPerm: the reader never writes through this source, and asking for
      // less than that is what lets it open a file another process still has
      // open for reading elsewhere.
      const OSErr err = FSpOpenDF(&spec, fsRdPerm, &refNum);
      if (err != noErr)
      {
        return false;
      }
      refNum_ = refNum;
      open_ = true;
      return true;
    }

    void ToolboxByteSource::close()
    {
      if (open_)
      {
        FSClose(refNum_);
        refNum_ = 0;
        open_ = false;
      }
    }

    bool ToolboxByteSource::readAt(std::size_t at, unsigned char *dst, std::size_t n)
    {
      if (!open_)
      {
        return false;
      }
      if (n == 0)
      {
        // Nothing to deliver, so nothing can fail -- and no seek, which
        // keeps a zero-length bag from being refused by a position the file
        // mark cannot express.
        return true;
      }
      if (!dst)
      {
        return false;
      }
      // SetFPos addresses with a signed long, and FSRead's count is one too.
      // A position or a length neither can express is a transport limit,
      // reported as such rather than truncated into a read of the wrong
      // bytes.
      if (at > static_cast<std::size_t>(LONG_MAX) || n > static_cast<std::size_t>(LONG_MAX))
      {
        return false;
      }
      if (SetFPos(refNum_, fsFromStart, static_cast<long>(at)) != noErr)
      {
        return false;
      }
      // FSRead reports eofErr when fewer bytes remain than were asked for,
      // updating `count` to what it actually transferred. Any other error is
      // a hard failure; eofErr is checked the same way everything else is --
      // by whether the count it left behind reached `n`.
      long count = static_cast<long>(n);
      const OSErr err = FSRead(refNum_, &count, dst);
      if (err != noErr && err != eofErr)
      {
        return false;
      }
      // Complete read or nothing: the interface has no partial answer, so a
      // short count is a failure here rather than a number the caller has to
      // loop on.
      return count >= 0 && static_cast<std::size_t>(count) == n;
    }

    bool ToolboxByteSource::size(std::size_t &out)
    {
      out = 0;
      if (!open_)
      {
        return false;
      }
      long eof = 0;
      if (GetEOF(refNum_, &eof) != noErr)
      {
        return false;
      }
      // GetEOF's logical end-of-file is a signed long, so a file whose real
      // size cannot be held in one (over ~2 GB, impossible on the media this
      // runs against but not ruled out by the type) reports negative here
      // rather than a wrapped positive number -- the false path closes on
      // its own.
      if (eof < 0)
      {
        return false;
      }
      out = static_cast<std::size_t>(eof);
      return true;
    }
  } // namespace toolbox
} // namespace loka
