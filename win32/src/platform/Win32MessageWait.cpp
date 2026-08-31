#include "platform/Win32MessageWait.hpp"

namespace
{
  const DWORD kCreateWaitableTimerHighResolution = 0x00000002;

  typedef HANDLE(WINAPI *CreateWaitableTimerExWProc)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);

  HANDLE CreateBestWaitableTimer()
  {
    HANDLE timer = NULL;
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    if (kernel)
    {
      // XP has no CreateWaitableTimerExW, and hosts before Windows 10 1803 do
      // not accept the high-resolution flag. Both cases use the ordinary
      // waitable-timer path below without adding a hard import.
      CreateWaitableTimerExWProc createTimerEx =
          reinterpret_cast<CreateWaitableTimerExWProc>(GetProcAddress(kernel, "CreateWaitableTimerExW"));
      if (createTimerEx)
      {
        timer = createTimerEx(NULL, NULL, kCreateWaitableTimerHighResolution, TIMER_MODIFY_STATE | SYNCHRONIZE);
      }
    }
    if (!timer)
    {
      timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    }
    return timer;
  }

  DWORD CounterTicksToTimeoutMilliseconds(LONGLONG ticks, LONGLONG frequency)
  {
    if (ticks <= 0 || frequency <= 0)
    {
      return 0;
    }
    const double milliseconds = static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency);
    if (milliseconds >= static_cast<double>(INFINITE - 1))
    {
      return INFINITE - 1;
    }
    DWORD timeout = static_cast<DWORD>(milliseconds);
    if (static_cast<double>(timeout) < milliseconds)
    {
      ++timeout;
    }
    return timeout > 0 ? timeout : 1;
  }

  LONGLONG CounterTicksToRelativeHundredNanoseconds(LONGLONG ticks, LONGLONG frequency)
  {
    const double hundredNanoseconds = static_cast<double>(ticks) * 10000000.0 / static_cast<double>(frequency);
    LONGLONG relative = static_cast<LONGLONG>(hundredNanoseconds);
    if (static_cast<double>(relative) < hundredNanoseconds)
    {
      ++relative;
    }
    return relative > 0 ? relative : 1;
  }

  void WaitForMessageOrTimeout(DWORD timeoutMilliseconds)
  {
    MsgWaitForMultipleObjects(0, NULL, FALSE, timeoutMilliseconds, QS_ALLINPUT);
  }
} // namespace

loka::platform::Win32MessageWait::Win32MessageWait()
    : timer_(CreateBestWaitableTimer())
{
}

loka::platform::Win32MessageWait::~Win32MessageWait()
{
  if (this->timer_)
  {
    CloseHandle(this->timer_);
    this->timer_ = NULL;
  }
}

void loka::platform::Win32MessageWait::waitUntil(LONGLONG deadlineCounter, LONGLONG counterFrequency)
{
  LARGE_INTEGER now;
  if (counterFrequency <= 0 || !QueryPerformanceCounter(&now))
  {
    WaitForMessageOrTimeout(1);
    return;
  }

  LONGLONG remainingTicks = deadlineCounter - now.QuadPart;
  if (remainingTicks <= 0)
  {
    return;
  }

  if (this->timer_)
  {
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -CounterTicksToRelativeHundredNanoseconds(remainingTicks, counterFrequency);
    if (SetWaitableTimer(this->timer_, &dueTime, 0, NULL, NULL, FALSE))
    {
      HANDLE handles[1] = {this->timer_};
      const DWORD result = MsgWaitForMultipleObjects(1, handles, FALSE, INFINITE, QS_ALLINPUT);
      CancelWaitableTimer(this->timer_);
      if (result != WAIT_FAILED)
      {
        return;
      }
    }
  }

  if (!QueryPerformanceCounter(&now))
  {
    WaitForMessageOrTimeout(1);
    return;
  }
  remainingTicks = deadlineCounter - now.QuadPart;
  WaitForMessageOrTimeout(CounterTicksToTimeoutMilliseconds(remainingTicks, counterFrequency));
}
