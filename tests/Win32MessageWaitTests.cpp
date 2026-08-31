#include "Win32MessageWaitTests.hpp"

#include "platform/Win32MessageWait.hpp"
#include "support/TestVerify.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

namespace
{
  void DrainCurrentThreadMessages()
  {
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
    }
  }

  double SecondsBetween(LONGLONG start, LONGLONG finish, LONGLONG frequency)
  {
    return static_cast<double>(finish - start) / static_cast<double>(frequency);
  }
} // namespace

void testWin32MessageWaitHonorsCounterDeadline()
{
  DrainCurrentThreadMessages();
  LARGE_INTEGER frequency;
  LARGE_INTEGER start;
  LOKA_VERIFY(QueryPerformanceFrequency(&frequency));
  LOKA_VERIFY(QueryPerformanceCounter(&start));

  loka::platform::Win32MessageWait wait;
  const LONGLONG deadline = start.QuadPart + frequency.QuadPart / 50;
  wait.waitUntil(deadline, frequency.QuadPart);

  LARGE_INTEGER finish;
  LOKA_VERIFY(QueryPerformanceCounter(&finish));
  const double elapsed = SecondsBetween(start.QuadPart, finish.QuadPart, frequency.QuadPart);
  LOKA_VERIFY(elapsed >= 0.005);
  LOKA_VERIFY(elapsed < 1.0);
  std::printf("testWin32MessageWaitHonorsCounterDeadline passed (%.3f ms)\n", elapsed * 1000.0);
}

void testWin32MessageWaitWakesForQueuedMessage()
{
  DrainCurrentThreadMessages();
  MSG message;
  PeekMessageW(&message, NULL, WM_USER + 296, WM_USER + 296, PM_NOREMOVE);
  LOKA_VERIFY(PostThreadMessageW(GetCurrentThreadId(), WM_USER + 296, 0, 0));

  LARGE_INTEGER frequency;
  LARGE_INTEGER start;
  LOKA_VERIFY(QueryPerformanceFrequency(&frequency));
  LOKA_VERIFY(QueryPerformanceCounter(&start));

  loka::platform::Win32MessageWait wait;
  wait.waitUntil(start.QuadPart + frequency.QuadPart, frequency.QuadPart);

  LARGE_INTEGER finish;
  LOKA_VERIFY(QueryPerformanceCounter(&finish));
  const double elapsed = SecondsBetween(start.QuadPart, finish.QuadPart, frequency.QuadPart);
  LOKA_VERIFY(elapsed < 0.2);
  LOKA_VERIFY(PeekMessageW(&message, NULL, WM_USER + 296, WM_USER + 296, PM_REMOVE));
  LOKA_VERIFY(message.message == WM_USER + 296);
  std::printf("testWin32MessageWaitWakesForQueuedMessage passed (%.3f ms)\n", elapsed * 1000.0);
}
