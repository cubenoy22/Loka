#ifndef LOKA_FILE_FILE_HPP
#define LOKA_FILE_FILE_HPP

#include "loka/core/String.hpp"
#include <cstring>

namespace loka
{
  namespace file
  {
    struct File
    {
      enum Kind
      {
        KIND_UNKNOWN = 0,
        KIND_FILE,
        KIND_FOLDER
      };

      enum BaseKind
      {
        BASE_NONE = 0,
        BASE_PATH,
        BASE_DESKTOP,
        BASE_DOCUMENTS,
        BASE_ROOT
      };

      File()
          : base_(BASE_NONE),
            kind_(KIND_UNKNOWN),
            basePath_(),
            relative_()
      {
      }

      explicit File(const char *name)
          : base_(BASE_NONE),
            kind_(KIND_UNKNOWN),
            basePath_(),
            relative_(loka::core::String::Literal(name ? name : ""))
      {
      }

      explicit File(const loka::core::String &name)
          : base_(BASE_NONE),
            kind_(KIND_UNKNOWN),
            basePath_(),
            relative_(name)
      {
      }

      static File Desktop()
      {
        File item;
        item.base_ = BASE_DESKTOP;
        return item;
      }

      static File Documents()
      {
        File item;
        item.base_ = BASE_DOCUMENTS;
        return item;
      }

      static File Root()
      {
        File item;
        item.base_ = BASE_ROOT;
        return item;
      }

#if !defined(LOKA_RETRO68)
      static File FromPath(const loka::core::String &value)
      {
        File item;
        item.base_ = BASE_PATH;
        item.basePath_ = value;
        return item;
      }

      static File FromPath(const char *value)
      {
        return FromPath(loka::core::String::Literal(value ? value : ""));
      }
#endif

      File operator<<(const File &child) const
      {
        if (child.base_ != BASE_NONE)
        {
          return child;
        }
        File combined(*this);
        combined.appendRelative(child.relative_);
        if (child.kind_ != KIND_UNKNOWN)
        {
          combined.kind_ = child.kind_;
        }
        return combined;
      }

      Kind kind() const
      {
        return this->kind_;
      }

      void setKind(Kind kind)
      {
        this->kind_ = kind;
      }

      loka::core::String toString() const
      {
        if (this->base_ == BASE_ROOT)
        {
          if (this->relative_.empty())
          {
            return this->separatorString();
          }
          return this->separatorString() + this->relative_;
        }
        const loka::core::String base = this->baseString();
        if (base.empty())
        {
          return this->relative_;
        }
        if (this->relative_.empty())
        {
          return base;
        }
        return base + this->separatorString() + this->relative_;
      }

    private:
      friend bool operator!=(const File &lhs, const File &rhs);

      BaseKind base_;
      Kind kind_;
      loka::core::String basePath_;
      loka::core::String relative_;

      static const char *separatorLiteral()
      {
#if defined(LOKA_RETRO68)
        return ":";
#else
        return "/";
#endif
      }

      loka::core::String separatorString() const
      {
        return loka::core::String::Literal(separatorLiteral());
      }

      loka::core::String baseString() const
      {
        if (this->base_ == BASE_PATH)
        {
          return this->basePath_;
        }
#if defined(LOKA_RETRO68)
        if (this->base_ == BASE_DESKTOP)
        {
          return loka::core::String::Literal("Desktop");
        }
        if (this->base_ == BASE_DOCUMENTS)
        {
          return loka::core::String::Literal("Documents");
        }
#else
        if (this->base_ == BASE_DESKTOP)
        {
          return loka::core::String::Literal("~/Desktop");
        }
        if (this->base_ == BASE_DOCUMENTS)
        {
          return loka::core::String::Literal("~/Documents");
        }
#endif
        return loka::core::String();
      }

      void appendRelative(const loka::core::String &segment)
      {
        if (segment.empty())
        {
          return;
        }
        if (this->relative_.empty())
        {
          this->relative_ = segment;
          return;
        }
        this->relative_ = this->relative_ + this->separatorString() + segment;
      }
    };

    inline bool operator!=(const File &lhs, const File &rhs)
    {
      if (lhs.kind_ != rhs.kind_)
      {
        return true;
      }
      if (lhs.base_ != rhs.base_)
      {
        return true;
      }
      if (lhs.basePath_.equals(rhs.basePath_) == false)
      {
        return true;
      }
      if (lhs.relative_.equals(rhs.relative_) == false)
      {
        return true;
      }
      return false;
    }

  }   // namespace file
} // namespace loka

#endif // LOKA_FILE_FILE_HPP
