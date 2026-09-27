#ifndef LOKA_APP_FOCUS_BINDING_HPP
#define LOKA_APP_FOCUS_BINDING_HPP

#include "app/FocusFact.hpp"
#include "app/scene/state/WriteSeat.hpp"
#include <functional>
#include <new>

namespace loka
{
  namespace app
  {

    /** Borrowed, erased report and request capability. Owns typed seats, never their states.
        Inline storage is aligned for the two pointer members of every WriteSeat.
        The per-key table preserves that specialization's transaction semantics. */
    class FocusBinding
    {
      union SeatStorage
      {
        void *alignment;
        char bytes[2 * sizeof(scene::WriteSeat<Focused<unsigned int> >)];
      };
      struct Functions
      {
        void (*copy)(void *, const void *);
        void (*destroy)(void *);
        const void *(*state)(const void *);
        void (*write)(const void *, FocusKeyWord, bool);
        bool (*equal)(const void *, FocusKeyWord, bool);
        const void *(*request)(const void *);
        bool (*match)(const void *, FocusKeyWord);
        void (*consume)(const void *);
        bool (*usesTracker)(const void *, const core::StateTracker *);
      };
      template <typename K> struct Operations
      {
        struct Seat
        {
          scene::WriteSeat<Focused<K> > fact;
          scene::WriteSeat<detail::FocusTarget<K> > request;
          Seat(const scene::WriteSeat<Focused<K> > &f,
               const scene::WriteSeat<detail::FocusTarget<K> > &r) : fact(f), request(r) {}
        };
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
          return static_cast<const Seat *>(seat)->fact.state();
        }
        static void write(const void *seat, FocusKeyWord word, bool held)
        {
          const Seat value = *static_cast<const Seat *>(seat);
          const Focused<K> next = held ? Focused<K>(FocusKeyTraits<K>::fromWord(word)) : Focused<K>::none();
          value.fact.set(next);
        }
        static bool equal(const void *seat, FocusKeyWord word, bool held)
        {
          const Seat &value = *static_cast<const Seat *>(seat);
          const Focused<K> next = held ? Focused<K>(FocusKeyTraits<K>::fromWord(word)) : Focused<K>::none();
          return !(value.fact.state()->get() != next);
        }
        static const void *request(const void *seat)
        {
          return static_cast<const Seat *>(seat)->request.state();
        }
        static bool usesTracker(const void *seat, const core::StateTracker *tracker)
        {
          const Seat &value = *static_cast<const Seat *>(seat);
          return value.fact.usesTracker(tracker) && value.request.usesTracker(tracker);
        }
        static bool match(const void *seat, FocusKeyWord word)
        {
          return static_cast<const Seat *>(seat)->request.state()->get().is(FocusKeyTraits<K>::fromWord(word));
        }
        static void consume(const void *seat)
        {
          const scene::WriteSeat<detail::FocusTarget<K> > request = static_cast<const Seat *>(seat)->request;
          if (!request.state()->get().isNone()) request.set(detail::FocusTarget<K>::None());
        }
        static const Functions *table()
        {
          static const Functions result = {&copy, &destroy, &state, &write, &equal, &request, &match, &consume, &usesTracker};
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
      FocusBinding(const scene::WriteSeat<Focused<K> > &seat, const scene::WriteSeat<detail::FocusTarget<K> > &request, K key)
          : functions_(0),
            key_(0)
      {
        typedef char SeatMustFit[(sizeof(typename Operations<K>::Seat) <= sizeof(SeatStorage)) ? 1 : -1];
        (void)sizeof(SeatMustFit);
        const FocusKeyWord word = FocusKeyTraits<K>::toWord(key);
        if (seat.isValid() && request.isValid())
        {
          this->functions_ = Operations<K>::table();
          this->key_ = word;
          new (this->storage_.bytes) typename Operations<K>::Seat(seat, request);
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
        return this->sameFact(other) && this->request() == other.request() && this->key_ == other.key_;
      }
      bool operator<(const FocusBinding &other) const
      {
        if (!this->sameFact(other))
          return std::less<const void *>()(this->state(), other.state());
        if (this->request() != other.request())
          return std::less<const void *>()(this->request(), other.request());
        return this->key_ < other.key_;
      }
      const void *request() const
      {
        return this->functions_ ? this->functions_->request(this->storage_.bytes) : 0;
      }
      bool usesTracker(const core::StateTracker *tracker) const
      {
        return this->functions_ && this->functions_->usesTracker(this->storage_.bytes, tracker);
      }
      bool requested() const
      {
        return this->functions_ && this->functions_->match(this->storage_.bytes, this->key_);
      }
      void consume() const
      {
        if (this->functions_) this->functions_->consume(this->storage_.bytes);
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
