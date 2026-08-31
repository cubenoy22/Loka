#ifndef LOKA_PLATFORM_WIN32_IDLE_PACER_HPP
#define LOKA_PLATFORM_WIN32_IDLE_PACER_HPP

#include "app/core/AppConfigurable.hpp"
#include <windows.h>

namespace loka
{
  namespace platform
  {
    /** Gates Win32 every-tick delivery and waits for either its next QPC
        deadline or new message input.

        The implementation prefers a high-resolution waitable timer when the
        host provides one and falls back to a message-aware ordinary timeout.
        The owned timer never changes the process-wide timer period. */
    class Win32IdlePacer
    {
    public:
      Win32IdlePacer();
      ~Win32IdlePacer();

      void reset();
      bool gateEveryTick(double candidateElapsedSeconds,
                         const loka::app::IdlePolicy &policy,
                         LONGLONG observedCounter,
                         LONGLONG counterFrequency,
                         double &dispatchElapsedSeconds);
      void wait(const loka::app::IdlePolicy &policy,
                bool idleDispatched,
                LONGLONG observedCounter,
                LONGLONG counterFrequency);

    private:
      Win32IdlePacer(const Win32IdlePacer &);
      Win32IdlePacer &operator=(const Win32IdlePacer &);

      void synchronizePolicy(const loka::app::IdlePolicy &policy, LONGLONG observedCounter, LONGLONG counterFrequency);
      void waitUntil(LONGLONG deadlineCounter, LONGLONG counterFrequency);

      HANDLE timer_;
      loka::app::IdlePolicy policy_;
      LONGLONG deadlineCounter_;
      double everyTickElapsedSeconds_;
    };
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_WIN32_IDLE_PACER_HPP
