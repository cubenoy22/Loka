#ifndef LOKA_APP_FOCUSED_HPP
#define LOKA_APP_FOCUSED_HPP

#include <cassert>
#include <cstddef>
#include <limits>
#include "core/ObservableList.hpp"

namespace loka
{
  namespace app
  {

    /** Explicit opt-in for lossless one-word keys. Specialize for application enums
        and integer IDs with toWord(K) and fromWord(Word). */
    template <typename K> struct FocusKeyTraits;
    typedef std::size_t FocusKeyWord;

    /** Reusable conversion for unsigned IDs and nonnegative application enums.
        Applications with signed or custom IDs supply their own lossless mapping. */
    template <typename K> struct UnsignedFocusKeyTraits
    {
      static FocusKeyWord toWord(K key)
      {
        typedef char KeyMustFit[(sizeof(K) <= sizeof(FocusKeyWord)) ? 1 : -1];
        (void)sizeof(KeyMustFit);
        return static_cast<FocusKeyWord>(key);
      }
      static K fromWord(FocusKeyWord word)
      {
        return static_cast<K>(word);
      }
    };
    template <> struct FocusKeyTraits<unsigned int> : UnsignedFocusKeyTraits<unsigned int>
    {
    };
    template <> struct FocusKeyTraits<unsigned long> : UnsignedFocusKeyTraits<unsigned long>
    {
    };
    template <> struct FocusKeyTraits<unsigned short> : UnsignedFocusKeyTraits<unsigned short>
    {
    };

    /** Signed IDs use the top of the word for negative values, without relying
        on implementation-defined out-of-range unsigned-to-signed conversion. */
    template <typename K> struct SignedFocusKeyTraits
    {
      static FocusKeyWord toWord(K key)
      {
        typedef char KeyMustFit[(sizeof(K) <= sizeof(FocusKeyWord)) ? 1 : -1];
        (void)sizeof(KeyMustFit);
        return key < 0 ? ~static_cast<FocusKeyWord>(-(key + 1)) : static_cast<FocusKeyWord>(key);
      }
      static K fromWord(FocusKeyWord word)
      {
        return word <= static_cast<FocusKeyWord>((std::numeric_limits<K>::max)())
                   ? static_cast<K>(word)
                   : static_cast<K>(-1 - static_cast<K>(~word));
      }
    };
    template <> struct FocusKeyTraits<int> : SignedFocusKeyTraits<int>
    {
    };
    template <> struct FocusKeyTraits<long> : SignedFocusKeyTraits<long>
    {
    };
    template <> struct FocusKeyTraits<short> : SignedFocusKeyTraits<short>
    {
    };
    template <> struct FocusKeyTraits<core::ItemId>
    {
      static FocusKeyWord toWord(core::ItemId key)
      {
        assert(!key.isNone() && "ItemId::none is not a held focus key");
        return (static_cast<FocusKeyWord>(key.generation) << 16) | key.seq;
      }
      static core::ItemId fromWord(FocusKeyWord word)
      {
        return core::ItemId(static_cast<unsigned short>(word >> 16), static_cast<unsigned short>(word & 0xffff));
      }
    };

    /** Screen-level focus fact. None has no key; key() requires a held value.
        A LazyFlex item's fact and key stay fixed for its structural lifetime. */
    template <typename K> class Focused
    {
    public:
      Focused()
          : key_(),
            held_(false)
      {
      }
      explicit Focused(K key)
          : key_(key),
            held_(true)
      {
        (void)FocusKeyTraits<K>::toWord(key);
      }
      static Focused none()
      {
        return Focused();
      }
      bool is(K key) const
      {
        return this->held_ && this->key_ == key;
      }
      K key() const
      {
        assert(this->held_);
        return this->key_;
      }
      bool operator!=(const Focused &other) const
      {
        return this->held_ != other.held_ || (this->held_ && this->key_ != other.key_);
      }

    private:
      K key_;
      bool held_;
    };

  } // namespace app
} // namespace loka
#endif
