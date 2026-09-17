#ifndef LOKA_APP_STYLE_STYLE_HPP
#define LOKA_APP_STYLE_STYLE_HPP

#include "app/style/StyleVocab.hpp"

namespace loka
{
  namespace app
  {
    /** Character-level text style. Unset fields are transparent during merge. */
    struct TextStyle
    {
      TextStyle()
          : fontSize_(0),
            weight_(TEXT_WEIGHT_NORMAL),
            italic_(false),
            hasFontSize_(false),
            hasWeight_(false),
            hasItalic_(false)
      {
      }

      TextStyle &weight(TextWeight value)
      {
        this->weight_ = value;
        this->hasWeight_ = true;
        return *this;
      }

      TextStyle &italic(bool value = true)
      {
        this->italic_ = value;
        this->hasItalic_ = true;
        return *this;
      }

      TextStyle operator+(const TextStyle &right) const
      {
        TextStyle result(*this);
        if (right.hasFontSize_)
        {
          result.fontSize_ = right.fontSize_;
          result.hasFontSize_ = true;
        }
        if (right.hasWeight_)
        {
          result.weight_ = right.weight_;
          result.hasWeight_ = true;
        }
        if (right.hasItalic_)
        {
          result.italic_ = right.italic_;
          result.hasItalic_ = true;
        }
        return result;
      }

      bool operator==(const TextStyle &other) const
      {
        return this->fontSize_ == other.fontSize_
               && this->weight_ == other.weight_
               && this->italic_ == other.italic_
               && this->hasFontSize_ == other.hasFontSize_
               && this->hasWeight_ == other.hasWeight_
               && this->hasItalic_ == other.hasItalic_;
      }

      bool operator!=(const TextStyle &other) const
      {
        return !(*this == other);
      }

      bool operator<(const TextStyle &other) const
      {
        if (this->fontSize_ != other.fontSize_)
          return this->fontSize_ < other.fontSize_;
        if (this->weight_ != other.weight_)
          return this->weight_ < other.weight_;
        if (this->italic_ != other.italic_)
          return this->italic_ < other.italic_;
        if (this->hasFontSize_ != other.hasFontSize_)
          return this->hasFontSize_ < other.hasFontSize_;
        if (this->hasWeight_ != other.hasWeight_)
          return this->hasWeight_ < other.hasWeight_;
        return this->hasItalic_ < other.hasItalic_;
      }

      int fontSize_;
      TextWeight weight_;
      bool italic_;
      bool hasFontSize_;
      bool hasWeight_;
      bool hasItalic_;
    };

    /** Paragraph-level text style. Unset fields are transparent during merge. */
    struct BlockStyle
    {
      BlockStyle()
          : wrap_(TEXT_WRAP_NONE),
            truncation_(TEXT_TRUNCATION_NONE),
            hasWrap_(false),
            hasTruncation_(false)
      {
      }

      BlockStyle &wrap(TextWrap value)
      {
        this->wrap_ = value;
        this->hasWrap_ = true;
        return *this;
      }

      BlockStyle &truncation(TextTruncation value)
      {
        this->truncation_ = value;
        this->hasTruncation_ = true;
        return *this;
      }

      BlockStyle operator+(const BlockStyle &right) const
      {
        BlockStyle result(*this);
        if (right.hasWrap_)
        {
          result.wrap_ = right.wrap_;
          result.hasWrap_ = true;
        }
        if (right.hasTruncation_)
        {
          result.truncation_ = right.truncation_;
          result.hasTruncation_ = true;
        }
        return result;
      }

      bool operator==(const BlockStyle &other) const
      {
        return this->wrap_ == other.wrap_
               && this->truncation_ == other.truncation_
               && this->hasWrap_ == other.hasWrap_
               && this->hasTruncation_ == other.hasTruncation_;
      }

      bool operator!=(const BlockStyle &other) const
      {
        return !(*this == other);
      }

      bool operator<(const BlockStyle &other) const
      {
        if (this->wrap_ != other.wrap_)
          return this->wrap_ < other.wrap_;
        if (this->truncation_ != other.truncation_)
          return this->truncation_ < other.truncation_;
        if (this->hasWrap_ != other.hasWrap_)
          return this->hasWrap_ < other.hasWrap_;
        return this->hasTruncation_ < other.hasTruncation_;
      }

      TextWrap wrap_;
      TextTruncation truncation_;
      bool hasWrap_;
      bool hasTruncation_;
    };
  } // namespace app
} // namespace loka

#define LOKA_STYLE_VOCAB_DEFINE_SIZES
#include "app/style/StyleVocab.hpp"
#undef LOKA_STYLE_VOCAB_DEFINE_SIZES

namespace loka
{
  namespace app
  {
    TextStyle SizeOf(int logicalUnits);

    extern const TextStyle Bold;
    extern const TextStyle Italic;
    extern const TextStyle Body;
    extern const TextStyle Caption;
    extern const TextStyle Heading;
    extern const TextStyle Title;
  } // namespace app
} // namespace loka

#endif // LOKA_APP_STYLE_STYLE_HPP
