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

      /** Owns a run window only when the tracker is not already running.
          Nested scopes are inert; only the owner publishes after-run
          requests and closes the window. The tracker must outlive the
          scope. */
      class RunScope
      {
      public:
        explicit RunScope(NextTickTracker &tracker)
            : owner_(tracker.inProgress_ ? 0 : &tracker)
        {
          if (this->owner_)
            this->owner_->inProgress_ = true;
        }

        ~RunScope()
        {
          if (!this->owner_)
            return;
          if (this->owner_->requestAfterRun_)
          {
            this->owner_->requestAfterRun_ = false;
            this->owner_->request();
          }
          this->owner_->inProgress_ = false;
        }

      private:
        RunScope(const RunScope &);
        RunScope &operator=(const RunScope &);
        NextTickTracker *owner_;
      };

      NextTickTracker()
          : requested_(false),
            inProgress_(false),
            requestAfterRun_(false),
            maxIterations_(100),
            pendingDelayMs_(0)
      {
      }

      static void RequestThunk(void *tracker)
      {
        NextTickTracker *nextTickTracker = static_cast<NextTickTracker *>(tracker);
        if (nextTickTracker)
        {
          nextTickTracker->request();
        }
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

      /** Refresh a requested batch; a null apply leaves projection to its caller. */
      bool run(RefreshFn refresh, ApplyFn apply, void *userData)
      {
        if (inProgress_)
        {
          return false;
        }
        RunScope run(*this);
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
        if (changed && apply)
        {
          // Keep the cycle closed across apply(): a request() that arrives
          // during apply must schedule the next run instead of re-entering
          // refresh for the current one.
          apply(userData);
        }
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
