#ifndef LOKA_PLATFORM_WIN32_MESSAGE_WAIT_HPP
#define LOKA_PLATFORM_WIN32_MESSAGE_WAIT_HPP

#include <windows.h>

namespace loka
{
  namespace platform
  {
    /** Waits for either a QPC deadline or new Win32 message input.

        The implementation prefers a high-resolution waitable timer when the
        host provides one and falls back to a message-aware ordinary timeout.
        The owned timer never changes the process-wide timer period. */
    class Win32MessageWait
    {
    public:
      Win32MessageWait();
      ~Win32MessageWait();

      void waitUntil(LONGLONG deadlineCounter, LONGLONG counterFrequency);

    private:
      Win32MessageWait(const Win32MessageWait &);
      Win32MessageWait &operator=(const Win32MessageWait &);

      HANDLE timer_;
    };
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_WIN32_MESSAGE_WAIT_HPP
