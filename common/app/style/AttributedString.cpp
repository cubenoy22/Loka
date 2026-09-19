#include "app/style/AttributedString.hpp"

#include <cassert>
#include <functional>
#include <limits>

namespace loka
{
  namespace app
  {
    namespace
    {
      const core::LokaAllocationSite kSegmentsSite("AttributedString", "Segments");

      // A cursor compares joined UTF-8 bytes without concatenating Strings or
      // materializing the whole line. Empty segments contribute no style.
      class ContentCursor
      {
      public:
        enum Kind
        {
          END,
          REFUSED,
          BYTE
        };
        explicit ContentCursor(const AttributedString &value)
            : value_(value),
              segment_(0),
              offset_(0),
              buffer_()
        {
        }

        Kind current()
        {
          while (this->segment_ < this->value_.segmentCount())
          {
            const core::String &text = this->value_.segment(this->segment_).text;
            if (this->offset_ == 0)
            {
              this->buffer_ = text.bufferWithEncoding(core::StringEncodingUtf8);
              if (!text.empty() && !this->buffer_.platformHandle().isValid())
                return REFUSED;
            }
            if (this->offset_ < this->buffer_.length())
              return BYTE;
            ++this->segment_;
            this->offset_ = 0;
          }
          return END;
        }

        unsigned int byte() const
        {
          return this->buffer_.characterAt(this->offset_);
        }
        const AttributedString::Segment &segment() const
        {
          return this->value_.segment(this->segment_);
        }
        void advance()
        {
          ++this->offset_;
        }

      private:
        const AttributedString &value_;
        std::size_t segment_;
        std::size_t offset_;
        core::StringBuffer buffer_;
      };
    } // namespace

    // The header and trailing placement-constructed array form one allocation.
    // Fundamental alignment suffices for Segment's pointer/int-based members
    // on the C++98 targets; the union keeps the trailing array aligned as well.
    struct AttributedString::Storage
    {
      union Header
      {
        std::size_t count;
        void *pointerAlignment;
        long double scalarAlignment;
      } header;

      explicit Storage(std::size_t count)
      {
        this->header.count = count;
      }
      Segment *segments()
      {
        return reinterpret_cast<Segment *>(this + 1);
      }
      const Segment *segments() const
      {
        return reinterpret_cast<const Segment *>(this + 1);
      }

      static void Release(Storage *storage, void *)
      {
        for (std::size_t i = storage->header.count; i > 0; --i)
          storage->segments()[i - 1].~Segment();
        storage->~Storage();
        core::LokaFreeRaw(storage, kSegmentsSite);
      }
    };

    AttributedString::AttributedString()
        : storage_(),
          valid_(true)
    {
    }
    bool AttributedString::valid() const
    {
      return this->valid_;
    }
    std::size_t AttributedString::segmentCount() const
    {
      return this->storage_.isValid() ? this->storage_->header.count : 0;
    }
    const AttributedString::Segment &AttributedString::segment(std::size_t index) const
    {
      assert(index < this->segmentCount());
      return this->storage_->segments()[index];
    }

    AttributedString AttributedString::Allocate(std::size_t count)
    {
#if __cplusplus >= 201103L
      static_assert(alignof(Storage) >= alignof(Segment), "Segment storage alignment");
#endif
      AttributedString result;
      result.valid_ = false;
      if (count > ((std::numeric_limits<std::size_t>::max)() - sizeof(Storage)) / sizeof(Segment))
        return result;
      void *memory = core::LokaAllocRaw(sizeof(Storage) + count * sizeof(Segment), kSegmentsSite);
      if (!memory)
        return result;
      Storage *storage = new (memory) Storage(count);
      for (std::size_t i = 0; i < count; ++i)
        new (storage->segments() + i) Segment();
      result.storage_ = core::Managed<Storage>::TryWrap(storage, &Storage::Release, 0);
      if (!result.storage_.isValid())
      {
        Storage::Release(storage, 0);
        return result;
      }
      result.valid_ = true;
      return result;
    }

    AttributedString Styled(const core::String &text, const TextStyle &style)
    {
      AttributedString result = AttributedString::Allocate(1);
      if (result.valid())
      {
        result.storage_->segments()[0].text = text;
        result.storage_->segments()[0].style = style;
      }
      return result;
    }

    AttributedString Styled(const char *text, const TextStyle &style)
    {
      return Styled(core::String::Literal(text), style);
    }

    AttributedString operator+(const AttributedString &left, const AttributedString &right)
    {
      if (!left.valid())
        return left;
      if (!right.valid())
        return right;
      const std::size_t leftCount = left.segmentCount();
      const std::size_t rightCount = right.segmentCount();
      if (rightCount > (std::numeric_limits<std::size_t>::max)() - leftCount)
      {
        AttributedString refused;
        refused.valid_ = false;
        return refused;
      }
      AttributedString result = AttributedString::Allocate(leftCount + rightCount);
      if (!result.valid())
        return result;
      for (std::size_t i = 0; i < leftCount + rightCount; ++i)
        result.storage_->segments()[i] = i < leftCount ? left.segment(i) : right.segment(i - leftCount);
      return result;
    }

    AttributedString AttributedString::fold(const TextStyle &defaults, const TextStyle &overrides) const
    {
      if (!this->valid())
        return *this;
      AttributedString result = Allocate(this->segmentCount());
      if (!result.valid())
        return result;
      for (std::size_t i = 0; i < this->segmentCount(); ++i)
      {
        result.storage_->segments()[i].text = this->segment(i).text;
        result.storage_->segments()[i].style = defaults + this->segment(i).style + overrides;
      }
      return result;
    }

    AttributedString operator+(const TextStyle &left, const AttributedString &right)
    {
      return right.fold(left, TextStyle());
    }
    AttributedString operator+(const AttributedString &left, const TextStyle &right)
    {
      return left.fold(TextStyle(), right);
    }

    bool AttributedString::empty() const
    {
      ContentCursor cursor(*this);
      return cursor.current() == ContentCursor::END;
    }

    int AttributedString::compare(const AttributedString &other) const
    {
      if (this->valid_ != other.valid_)
        return this->valid_ ? 1 : -1;
      if (this->storage_ == other.storage_)
        return 0;
      ContentCursor left(*this);
      ContentCursor right(other);
      for (;;)
      {
        const ContentCursor::Kind leftKind = left.current();
        const ContentCursor::Kind rightKind = right.current();
        if (leftKind != rightKind)
          return leftKind < rightKind ? -1 : 1;
        if (leftKind == ContentCursor::END)
          return 0;
        if (leftKind == ContentCursor::REFUSED)
          return std::less<const Segment *>()(&left.segment(), &right.segment()) ? -1 : 1;
        if (left.byte() != right.byte())
          return left.byte() < right.byte() ? -1 : 1;
        if (left.segment().style != right.segment().style)
          return left.segment().style < right.segment().style ? -1 : 1;
        left.advance();
        right.advance();
      }
    }

    bool AttributedString::equals(const AttributedString &other) const
    {
      return this->compare(other) == 0;
    }
    bool AttributedString::operator==(const AttributedString &other) const
    {
      return this->equals(other);
    }
    bool AttributedString::operator!=(const AttributedString &other) const
    {
      return !this->equals(other);
    }
  } // namespace app
} // namespace loka
