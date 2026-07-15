#ifndef LOKA_CORE2_SCENE_PROJECTION_NATIVE_HANDLE_POOL_HPP
#define LOKA_CORE2_SCENE_PROJECTION_NATIVE_HANDLE_POOL_HPP

#include <cstddef>
#include <vector>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** One exact-match bucket of the native deferred pool (W3-2, #98).
          A bucket holds ritual-complete native handles of a single creation
          recipe; payout is exact-match only — a miss means the platform
          creates a fresh handle, never adapts one from another bucket.
          Performance is earned manually: hit/miss/evict counters plus live
          depth are the observable evidence, surfaced through debugStats.

          The bucket never touches the handles it holds. Intake (which
          entries qualify), payout reconfiguration, and disposal all belong
          to the owning platform controller; drainWith hands each entry to
          the platform's dispose routine at the App-owner safe points. */
      template <typename HandleT> class ExactMatchHandleBucket
      {
      public:
        ExactMatchHandleBucket()
            : entries_(),
              hitCount_(0),
              missCount_(0),
              evictCount_(0)
        {
        }

        void offer(HandleT handle)
        {
          entries_.push_back(handle);
        }

        bool tryAcquire(HandleT &out)
        {
          if (entries_.empty())
          {
            ++missCount_;
            return false;
          }
          out = entries_.back();
          entries_.pop_back();
          ++hitCount_;
          return true;
        }

        template <typename DisposeFn> void drainWith(DisposeFn dispose)
        {
          for (std::size_t i = 0; i < entries_.size(); ++i)
          {
            dispose(entries_[i]);
          }
          evictCount_ += static_cast<unsigned long>(entries_.size());
          entries_.clear();
        }

        std::size_t depth() const
        {
          return entries_.size();
        }
        unsigned long hitCount() const
        {
          return hitCount_;
        }
        unsigned long missCount() const
        {
          return missCount_;
        }
        unsigned long evictCount() const
        {
          return evictCount_;
        }

      private:
        ExactMatchHandleBucket(const ExactMatchHandleBucket &);
        ExactMatchHandleBucket &operator=(const ExactMatchHandleBucket &);

        std::vector<HandleT> entries_;
        unsigned long hitCount_;
        unsigned long missCount_;
        unsigned long evictCount_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_NATIVE_HANDLE_POOL_HPP
