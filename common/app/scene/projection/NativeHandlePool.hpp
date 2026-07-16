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
        /** maxDepth bounds how many handles the bag may keep (0 = unbounded).
            The bound keeps peak-churn scenes from pinning their high-water
            mark of native handles for the controller's whole lifetime —
            a hard cap, not a scoring policy. */
        explicit ExactMatchHandleBucket(std::size_t maxDepth = 0)
            : entries_(),
              maxDepth_(maxDepth),
              hitCount_(0),
              missCount_(0),
              evictCount_(0)
        {
        }

        /** Returns false when the bucket is full; the caller keeps ownership
            and disposes the handle at its safe point. The refusal counts as
            an evict — an entry the pool decided not to keep. */
        bool offer(HandleT handle)
        {
          if (maxDepth_ != 0 && entries_.size() >= maxDepth_)
          {
            ++evictCount_;
            return false;
          }
          entries_.push_back(handle);
          return true;
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

        /** Zeroes the measurement counters without touching the bag itself —
            depth is a live gauge, not a measurement, so it survives. Pairs
            with the platform's debug-stats reset to bracket experiments. */
        void resetCounters()
        {
          hitCount_ = 0;
          missCount_ = 0;
          evictCount_ = 0;
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
        std::size_t maxDepth_;
        unsigned long hitCount_;
        unsigned long missCount_;
        unsigned long evictCount_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_NATIVE_HANDLE_POOL_HPP
