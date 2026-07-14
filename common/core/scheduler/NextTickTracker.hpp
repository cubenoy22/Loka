#ifndef LOKA_CORE_SCHEDULER_NEXT_TICK_TRACKER_HPP
#define LOKA_CORE_SCHEDULER_NEXT_TICK_TRACKER_HPP

namespace loka
{
  namespace core
  {
    class NextTickTracker
    {
    public:
      typedef bool (*RefreshFn)(void *);
      typedef void (*ApplyFn)(void *);

      NextTickTracker()
          : requested_(false),
            inProgress_(false),
            requestAfterRun_(false),
            maxIterations_(100),
            pendingDelayMs_(0)
      {
      }

      void request(unsigned long delayMs = 0)
      {
        if (!requested_)
        {
          requested_ = true;
          pendingDelayMs_ = delayMs;
          return;
        }
        // Keep the earliest requested execution time.
        if (delayMs < pendingDelayMs_)
        {
          pendingDelayMs_ = delayMs;
        }
      }

      bool inProgress() const
      {
        return inProgress_;
      }
      bool hasPendingRequest() const
      {
        return requested_;
      }
      /**
       * Schedules work for the next run without re-entering the current drain
       * loop. Outside a run this is equivalent to request().
       */
      void requestAfterRun()
      {
        if (!inProgress_)
        {
          this->request();
          return;
        }
        requested_ = false;
        pendingDelayMs_ = 0;
        requestAfterRun_ = true;
      }
      unsigned long pendingDelayMs() const
      {
        return pendingDelayMs_;
      }

      void setMaxIterations(int maxIterations)
      {
        maxIterations_ = maxIterations;
      }

      bool run(RefreshFn refresh, ApplyFn apply, void *userData)
      {
        if (inProgress_)
        {
          return false;
        }
        inProgress_ = true;
        bool changed = false;
        int iterations = 0;
        while (requested_ && iterations < maxIterations_)
        {
          requested_ = false;
          pendingDelayMs_ = 0;
          if (refresh(userData))
          {
            changed = true;
          }
          ++iterations;
        }
        if (changed)
        {
          // Keep the cycle closed across apply(): a request() that arrives
          // during apply must schedule the next run instead of re-entering
          // refresh for the current one.
          apply(userData);
        }
        if (requestAfterRun_)
        {
          requestAfterRun_ = false;
          this->request();
        }
        inProgress_ = false;
        return changed;
      }

    private:
      bool requested_;
      bool inProgress_;
      bool requestAfterRun_;
      int maxIterations_;
      unsigned long pendingDelayMs_;
    };
  } // namespace core
} // namespace loka

#endif // LOKA_CORE_SCHEDULER_NEXT_TICK_TRACKER_HPP
