#include "Win32IdlePacerTests.hpp"

#include "platform/Win32IdlePacer.hpp"
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

void testWin32IdlePacerHonorsCounterDeadline()
{
  DrainCurrentThreadMessages();
  LARGE_INTEGER frequency;
  LARGE_INTEGER start;
  LOKA_VERIFY(QueryPerformanceFrequency(&frequency));
  LOKA_VERIFY(QueryPerformanceCounter(&start));

  loka::platform::Win32IdlePacer pacer;
  pacer.wait(loka::app::IdlePolicy::interval(0.02), false, start.QuadPart, frequency.QuadPart);

  LARGE_INTEGER finish;
  LOKA_VERIFY(QueryPerformanceCounter(&finish));
  const double elapsed = SecondsBetween(start.QuadPart, finish.QuadPart, frequency.QuadPart);
  LOKA_VERIFY(elapsed >= 0.005);
  LOKA_VERIFY(elapsed < 1.0);
  std::printf("testWin32IdlePacerHonorsCounterDeadline passed (%.3f ms)\n", elapsed * 1000.0);
}

void testWin32IdlePacerWakesForQueuedMessage()
{
  DrainCurrentThreadMessages();
  MSG message;
  PeekMessageW(&message, NULL, WM_USER + 296, WM_USER + 296, PM_NOREMOVE);
  LOKA_VERIFY(PostThreadMessageW(GetCurrentThreadId(), WM_USER + 296, 0, 0));

  LARGE_INTEGER frequency;
  LARGE_INTEGER start;
  LOKA_VERIFY(QueryPerformanceFrequency(&frequency));
  LOKA_VERIFY(QueryPerformanceCounter(&start));

  loka::platform::Win32IdlePacer pacer;
  pacer.wait(loka::app::IdlePolicy::interval(1.0), false, start.QuadPart, frequency.QuadPart);

  LARGE_INTEGER finish;
  LOKA_VERIFY(QueryPerformanceCounter(&finish));
  const double elapsed = SecondsBetween(start.QuadPart, finish.QuadPart, frequency.QuadPart);
  LOKA_VERIFY(elapsed < 0.2);
  LOKA_VERIFY(PeekMessageW(&message, NULL, WM_USER + 296, WM_USER + 296, PM_REMOVE));
  LOKA_VERIFY(message.message == WM_USER + 296);
  std::printf("testWin32IdlePacerWakesForQueuedMessage passed (%.3f ms)\n", elapsed * 1000.0);
}

void testWin32EveryTickGateDoesNotDispatchOnEarlyMessages()
{
  const LONGLONG frequency = 60000;
  const LONGLONG start = 100000;
  const LONGLONG frameTicks = 1000;
  const loka::app::IdlePolicy policy = loka::app::IdlePolicy::everyTick();
  loka::platform::Win32IdlePacer pacer;
  double dispatchElapsedSeconds = 0.0;

  LOKA_VERIFY(!pacer.gateEveryTick(0.001, policy, start, frequency, dispatchElapsedSeconds));
  for (int message = 1; message < 10; ++message)
  {
    LOKA_VERIFY(!pacer.gateEveryTick(0.001, policy, start + message * 100, frequency, dispatchElapsedSeconds));
  }
  LOKA_VERIFY(pacer.gateEveryTick(0.001, policy, start + frameTicks, frequency, dispatchElapsedSeconds));
  LOKA_VERIFY(dispatchElapsedSeconds > 0.010 && dispatchElapsedSeconds < 0.012);

  LOKA_VERIFY(!pacer.gateEveryTick(0.001, policy, start + frameTicks + 100, frequency, dispatchElapsedSeconds));
  LOKA_VERIFY(pacer.gateEveryTick(0.009, policy, start + frameTicks * 2, frequency, dispatchElapsedSeconds));
  LOKA_VERIFY(dispatchElapsedSeconds > 0.009 && dispatchElapsedSeconds < 0.011);
  std::printf("testWin32EveryTickGateDoesNotDispatchOnEarlyMessages passed\n");
}
