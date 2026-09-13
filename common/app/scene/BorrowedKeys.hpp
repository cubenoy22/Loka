#ifndef LOKA_APP_SCENE_BORROWED_KEYS_HPP
#define LOKA_APP_SCENE_BORROWED_KEYS_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Fixed borrowed-source identities, compared by address rather than
          pointee value (#557 props-equivalence contract). Store this value as
          a member of an example's props, never inside PropsBase: base metadata
          is not part of the keys. Copies neither retain nor own the sources;
          their owner must outlive every use. N must be positive and set/get
          indices must be less than N. Null denotes an uninitialized key. */
      template <std::size_t N> class BorrowedKeys
      {
      public:
        BorrowedKeys()
        {
          for (std::size_t i = 0; i < N; ++i)
            this->keys_[i] = 0;
        }

        void set(std::size_t index, const void *key)
        {
          assert(index < N);
          this->keys_[index] = key;
        }

        const void *get(std::size_t index) const
        {
          assert(index < N);
          return this->keys_[index];
        }

        bool complete() const
        {
          for (std::size_t i = 0; i < N; ++i)
            if (!this->keys_[i])
              return false;
          return true;
        }

        bool operator<(const BorrowedKeys &other) const
        {
          return std::lexicographical_compare(
              this->keys_, this->keys_ + N, other.keys_, other.keys_ + N,
              std::less<const void *>());
        }

        bool operator==(const BorrowedKeys &other) const
        {
          return std::equal(this->keys_, this->keys_ + N, other.keys_);
        }

      private:
        const void *keys_[N];
      };
    }
  }
}

#endif // LOKA_APP_SCENE_BORROWED_KEYS_HPP
