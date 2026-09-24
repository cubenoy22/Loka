#ifndef LOKA_APP_FOCUS_BINDING_HPP
#define LOKA_APP_FOCUS_BINDING_HPP

#include "app/Focused.hpp"
#include "app/scene/state/WriteSeat.hpp"
#include <functional>
#include <new>

namespace loka
{
  namespace app
  {

    /** Borrowed, erased report capability. Owns a typed seat value, never its state.
        Inline storage is aligned for the two pointer members of every WriteSeat.
        The per-key table preserves that specialization's transaction semantics. */
    class FocusBinding
    {
      union SeatStorage
      {
        void *alignment;
        char bytes[sizeof(scene::WriteSeat<Focused<unsigned int> >)];
      };
      struct Functions
      {
        void (*copy)(void *, const void *);
        void (*destroy)(void *);
        const void *(*state)(const void *);
        void (*write)(const void *, FocusKeyWord, bool);
        bool (*equal)(const void *, FocusKeyWord, bool);
      };
      template <typename K> struct Operations
      {
        typedef scene::WriteSeat<Focused<K> > Seat;
        static void copy(void *to, const void *from)
        {
          new (to) Seat(*static_cast<const Seat *>(from));
        }
        static void destroy(void *seat)
        {
          static_cast<Seat *>(seat)->~Seat();
        }
        static const void *state(const void *seat)
        {
          return static_cast<const Seat *>(seat)->state();
        }
        static void write(const void *seat, FocusKeyWord word, bool held)
        {
          const Seat value = *static_cast<const Seat *>(seat);
          const Focused<K> next = held ? Focused<K>(FocusKeyTraits<K>::fromWord(word)) : Focused<K>::none();
          value.set(next);
        }
        static bool equal(const void *seat, FocusKeyWord word, bool held)
        {
          const Seat &value = *static_cast<const Seat *>(seat);
          const Focused<K> next = held ? Focused<K>(FocusKeyTraits<K>::fromWord(word)) : Focused<K>::none();
          return !(value.state()->get() != next);
        }
        static const Functions *table()
        {
          static const Functions result = {&copy, &destroy, &state, &write, &equal};
          return &result;
        }
      };

    public:
      FocusBinding()
          : functions_(0),
            key_(0)
      {
      }
      template <typename K>
      FocusBinding(const scene::WriteSeat<Focused<K> > &seat, K key)
          : functions_(0),
            key_(0)
      {
        typedef char SeatMustFit[(sizeof(seat) <= sizeof(SeatStorage)) ? 1 : -1];
        (void)sizeof(SeatMustFit);
        const FocusKeyWord word = FocusKeyTraits<K>::toWord(key);
        if (seat.isValid())
        {
          this->functions_ = Operations<K>::table();
          this->key_ = word;
          new (this->storage_.bytes) scene::WriteSeat<Focused<K> >(seat);
        }
      }
      FocusBinding(const FocusBinding &other)
          : functions_(other.functions_),
            key_(other.key_)
      {
        if (this->functions_)
          this->functions_->copy(this->storage_.bytes, other.storage_.bytes);
      }
      ~FocusBinding()
      {
        if (this->functions_)
          this->functions_->destroy(this->storage_.bytes);
      }
      FocusBinding &operator=(const FocusBinding &other)
      {
        if (this != &other)
        {
          if (this->functions_)
            this->functions_->destroy(this->storage_.bytes);
          this->functions_ = other.functions_;
          this->key_ = other.key_;
          if (this->functions_)
            this->functions_->copy(this->storage_.bytes, other.storage_.bytes);
        }
        return *this;
      }
      const void *state() const
      {
        return this->functions_ ? this->functions_->state(this->storage_.bytes) : 0;
      }
      bool sameFact(const FocusBinding &other) const
      {
        return this->state() == other.state();
      }
      bool same(const FocusBinding &other) const
      {
        return this->sameFact(other) && this->key_ == other.key_;
      }
      bool operator<(const FocusBinding &other) const
      {
        if (!this->sameFact(other))
          return std::less<const void *>()(this->state(), other.state());
        return this->key_ < other.key_;
      }
      void publish() const
      {
        if (this->functions_ && !this->functions_->equal(this->storage_.bytes, this->key_, true))
          this->functions_->write(this->storage_.bytes, this->key_, true);
      }
      void clear() const
      {
        if (this->functions_ && !this->functions_->equal(this->storage_.bytes, this->key_, false))
          this->functions_->write(this->storage_.bytes, this->key_, false);
      }

    private:
      SeatStorage storage_;
      const Functions *functions_;
      FocusKeyWord key_;
    };

  } // namespace app
} // namespace loka
#endif
