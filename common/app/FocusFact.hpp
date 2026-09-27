#ifndef LOKA_APP_FOCUS_FACT_HPP
#define LOKA_APP_FOCUS_FACT_HPP

#include "app/Focused.hpp"
#include "app/scene/state/Reported.hpp"
#include "app/scene/state/Request.hpp"

namespace loka
{
  namespace app
  {
    namespace detail
    {
      /** Internal latest-wins focus request value. */
      template <typename K> class FocusTarget
      {
      public:
        FocusTarget()
            : value_()
        {
        }
        explicit FocusTarget(K key)
            : value_(key)
        {
        }
        static FocusTarget None()
        {
          return FocusTarget();
        }
        bool isNone() const
        {
          return !(this->value_ != Focused<K>::none());
        }
        bool is(K key) const
        {
          return this->value_.is(key);
        }
        bool operator!=(const FocusTarget &other) const
        {
          return this->value_ != other.value_;
        }

      private:
        Focused<K> value_;
      };
    } // namespace detail
    namespace scene
    {
      template <typename K> struct RequestTraits< ::loka::app::detail::FocusTarget<K> >
      {
        enum
        {
          coalescable = 1
        };
      };
    } // namespace scene
    /** Scene-local app declaration: rail-written observation and app-posted request.
        Declare in an ancestor of borrowing fields. Copies would hide that ownership. */
    template <typename K> class FocusFact
    {
    public:
      FocusFact() {}
      bool isValid() const
      {
        return this->fact_.isValid() && this->request_.isValid();
      }
      core::State<Focused<K> > *state() const
      {
        return this->fact_.state();
      }
      void post(K key)
      {
        const detail::FocusTarget<K> target(key);
        if (this->isValid())
          this->request_.set(target);
      }

    private:
      FocusFact(const FocusFact &);
      FocusFact &operator=(const FocusFact &);
      scene::Reported<Focused<K> > fact_;
      scene::Request<detail::FocusTarget<K> > request_;
      friend class scene::ComposableNode;
      friend class scene::StateBatchBase;
      template <class PropsT> friend struct scene::NodePropsBase;
    };
  } // namespace app
} // namespace loka
#endif
