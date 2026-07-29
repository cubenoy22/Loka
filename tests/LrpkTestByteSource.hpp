#ifndef LOKA_TESTS_LRPKTESTBYTESOURCE_HPP
#define LOKA_TESTS_LRPKTESTBYTESOURCE_HPP

#include <cstddef>
#include <cstring>
#include <vector>

#include "core/resource/lrpk/LrpkReader.hpp"

namespace loka
{
  namespace lrpktests
  {
    /** A `ByteSource` over bytes the test already holds, with two knobs for
        making it lie.

        The failure knob is a byte *window*, not a call count. "Fail the third
        `readAt`" would pin how many probes `beginOpen` happens to make today,
        which is exactly the implementation detail the two-transport refusal
        table exists to be independent of; "these bytes cannot be delivered"
        names the fault instead and survives any reshuffling of the reads.

        Reading past the end answers false as well, so a source that is asked
        for bytes the format promised but the file does not have behaves like
        a real one rather than like a buffer overrun. */
    class MemoryByteSource : public loka::core::resource::lrpk::ByteSource
    {
    public:
      explicit MemoryByteSource(const std::vector<unsigned char> &bytes)
          : bytes_(bytes),
            failSize_(false),
            reportedSize_(0),
            hasReportedSize_(false),
            failBegin_(0),
            failEnd_(0)
      {
      }

      /** Makes `size()` answer false, the shape a 32-bit host takes when a
          file is larger than `std::size_t` can describe. */
      void failSize() { failSize_ = true; }

      /** Makes `size()` answer a total these bytes do not have, so the
          reader's own range gates can be reached without allocating a file
          that large. */
      void reportSize(std::size_t value)
      {
        reportedSize_ = value;
        hasReportedSize_ = true;
      }

      /** Every `readAt` overlapping `[begin, end)` answers false. An empty
          window disarms the knob. */
      void failReadsOver(std::size_t begin, std::size_t end)
      {
        failBegin_ = begin;
        failEnd_ = end;
      }

      virtual bool readAt(std::size_t at, unsigned char *dst, std::size_t n)
      {
        if (at > bytes_.size() || bytes_.size() - at < n)
        {
          return false;
        }
        if (n > 0 && failEnd_ > failBegin_ && at < failEnd_ && failBegin_ < at + n)
        {
          return false;
        }
        if (n > 0)
        {
          std::memcpy(dst, &bytes_[at], n);
        }
        return true;
      }

      virtual bool size(std::size_t &out)
      {
        out = 0;
        if (failSize_)
        {
          return false;
        }
        out = hasReportedSize_ ? reportedSize_ : bytes_.size();
        return true;
      }

    private:
      MemoryByteSource(const MemoryByteSource &);
      MemoryByteSource &operator=(const MemoryByteSource &);

      const std::vector<unsigned char> &bytes_;
      bool failSize_;
      std::size_t reportedSize_;
      bool hasReportedSize_;
      std::size_t failBegin_;
      std::size_t failEnd_;
    };
  } // namespace lrpktests
} // namespace loka

#endif // LOKA_TESTS_LRPKTESTBYTESOURCE_HPP
