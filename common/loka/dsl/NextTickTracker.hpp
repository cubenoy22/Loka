#ifndef LOKA_DSL_NEXT_TICK_TRACKER_HPP
#define LOKA_DSL_NEXT_TICK_TRACKER_HPP

namespace loka
{
  namespace dsl
  {
    class NextTickTracker
    {
    public:
      typedef bool (*RefreshFn)(void *);
      typedef void (*ApplyFn)(void *);

      NextTickTracker()
          : requested_(false), inProgress_(false), maxIterations_(100), pendingDelayMs_(0) {}

      void request(unsigned long delayMs = 0)
      {
        requested_ = true;
        if (delayMs > pendingDelayMs_)
        {
          pendingDelayMs_ = delayMs;
        }
      }

      bool inProgress() const { return inProgress_; }
      bool hasPendingRequest() const { return requested_; }
      unsigned long pendingDelayMs() const { return pendingDelayMs_; }

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
        inProgress_ = false;
        if (changed)
        {
          apply(userData);
        }
        return changed;
      }

    private:
      bool requested_;
      bool inProgress_;
      int maxIterations_;
      unsigned long pendingDelayMs_;
    };
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_NEXT_TICK_TRACKER_HPP
