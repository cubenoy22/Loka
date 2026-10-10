#ifndef LOKA_KEY_SNAPSHOT_HPP
#define LOKA_KEY_SNAPSHOT_HPP

#include "core/State.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {
        /** One key value read at construction and compared against the live key
            later. It lives inside the declaration it describes: a committed
            declaration owns its key snapshot (KeyedSeatDesign.md). */
        template <class K> class KeySnapshot
        {
        public:
          explicit KeySnapshot(loka::core::State<K> *key) : key_(key), value_(key->get()) {}
          bool matches() const { return this->value_ == this->key_->get(); }

        private:
          loka::core::State<K> *const key_;
          const K value_;
        };
      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
