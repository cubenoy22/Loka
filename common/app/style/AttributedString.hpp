#ifndef LOKA_APP_STYLE_ATTRIBUTED_STRING_HPP
#define LOKA_APP_STYLE_ATTRIBUTED_STRING_HPP

#include "app/style/Style.hpp"
#include "core/String.hpp"

namespace loka
{
  namespace app
  {
    /** Immutable styled content with shared segment storage. */
    class AttributedString
    {
    public:
      /** One text contribution and its declared character style. */
      struct Segment
      {
        core::String text;
        TextStyle style;
      };

      /** Constructs a valid, empty value without allocating. */
      AttributedString();
      bool valid() const;
      /** Refused text buffers are conservatively considered nonempty. */
      bool empty() const;
      std::size_t segmentCount() const;
      /** Borrows a segment for this value's lifetime; index must be in range. */
      const Segment &segment(std::size_t index) const;
      bool equals(const AttributedString &other) const;
      bool operator==(const AttributedString &other) const;
      bool operator!=(const AttributedString &other) const;
      /**
       * Lexicographic styled UTF-8 ordering, independent of segmentation.
       * Invalid values precede valid ones. A refused buffer is an opaque token
       * before readable bytes, ordered by segment identity among refusals.
       * Distinct refused segments never compare equal; shared storage does.
       */
      int compare(const AttributedString &other) const;

    private:
      struct Storage;
      core::Managed<Storage> storage_;
      bool valid_;

      static AttributedString Allocate(std::size_t count);
      AttributedString fold(const TextStyle &defaults, const TextStyle &overrides) const;
      friend AttributedString Styled(const core::String &, const TextStyle &);
      friend AttributedString operator+(const AttributedString &, const AttributedString &);
      friend AttributedString operator+(const TextStyle &, const AttributedString &);
      friend AttributedString operator+(const AttributedString &, const TextStyle &);
    };

    /**
     * Shares the String's existing handle and copies its style. Refusal covers
     * only the segment array and its Managed control block; construction of the
     * supplied String (including the literal overload) keeps String's contract.
     */
    AttributedString Styled(const core::String &text, const TextStyle &style);
    /** Literal convenience form with the same storage-refusal contract. */
    AttributedString Styled(const char *text, const TextStyle &style);
    /** Concatenates segments atomically; an invalid operand propagates refusal. */
    AttributedString operator+(const AttributedString &left, const AttributedString &right);
    /** Supplies defaults; each segment's declared style wins. */
    AttributedString operator+(const TextStyle &left, const AttributedString &right);
    /** Overrides every segment; the right style wins. */
    AttributedString operator+(const AttributedString &left, const TextStyle &right);
  } // namespace app
} // namespace loka

#endif // LOKA_APP_STYLE_ATTRIBUTED_STRING_HPP
