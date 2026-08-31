#include "platform/Win32IdlePacer.hpp"

#include <limits>

namespace
{
  const DWORD kCreateWaitableTimerHighResolution = 0x00000002;
  // Message wakes service input but do not become application ticks. Keep the
  // every-tick policy near the display cadence used by the other UI pumps.
  const double kWin32EveryTickIntervalSeconds = 1.0 / 60.0;

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

  loka::app::IdlePolicy NormalizePolicy(const loka::app::IdlePolicy &policy)
  {
    loka::app::IdlePolicy normalized = policy;
    switch (normalized.mode)
    {
    case loka::app::IDLE_MODE_NONE:
      normalized.intervalSeconds = 0.0;
      break;
    case loka::app::IDLE_MODE_EVERY_TICK:
      normalized.intervalSeconds = kWin32EveryTickIntervalSeconds;
      break;
    case loka::app::IDLE_MODE_INTERVAL:
      if (normalized.intervalSeconds <= 0.0)
      {
        normalized.intervalSeconds = kWin32EveryTickIntervalSeconds;
      }
      break;
    }
    return normalized;
  }

  LONGLONG AddCounterInterval(LONGLONG counter, LONGLONG frequency, double intervalSeconds)
  {
    double counterTicks = intervalSeconds * static_cast<double>(frequency);
    if (counterTicks < 1.0)
    {
      counterTicks = 1.0;
    }
    const LONGLONG maximum = (std::numeric_limits<LONGLONG>::max)();
    if (counterTicks >= static_cast<double>(maximum - counter))
    {
      return maximum;
    }
    return counter + static_cast<LONGLONG>(counterTicks + 0.5);
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

loka::platform::Win32IdlePacer::Win32IdlePacer()
    : timer_(CreateBestWaitableTimer()),
      policy_(loka::app::IdlePolicy::none()),
      deadlineCounter_(0),
      everyTickElapsedSeconds_(0.0)
{
}

loka::platform::Win32IdlePacer::~Win32IdlePacer()
{
  if (this->timer_)
  {
    CloseHandle(this->timer_);
    this->timer_ = NULL;
  }
}

void loka::platform::Win32IdlePacer::reset()
{
  this->policy_ = loka::app::IdlePolicy::none();
  this->deadlineCounter_ = 0;
  this->everyTickElapsedSeconds_ = 0.0;
}

void loka::platform::Win32IdlePacer::synchronizePolicy(const loka::app::IdlePolicy &policy,
                                                       LONGLONG observedCounter,
                                                       LONGLONG counterFrequency)
{
  const loka::app::IdlePolicy normalized = NormalizePolicy(policy);
  if (this->policy_.mode == normalized.mode && this->policy_.intervalSeconds == normalized.intervalSeconds)
  {
    return;
  }
  this->policy_ = normalized;
  this->deadlineCounter_ = AddCounterInterval(observedCounter, counterFrequency, this->policy_.intervalSeconds);
  this->everyTickElapsedSeconds_ = 0.0;
}

bool loka::platform::Win32IdlePacer::gateEveryTick(double candidateElapsedSeconds,
                                                   const loka::app::IdlePolicy &policy,
                                                   LONGLONG observedCounter,
                                                   LONGLONG counterFrequency,
                                                   double &dispatchElapsedSeconds)
{
  if (policy.mode != loka::app::IDLE_MODE_EVERY_TICK)
  {
    return false;
  }
  this->synchronizePolicy(policy, observedCounter, counterFrequency);
  this->everyTickElapsedSeconds_ += candidateElapsedSeconds;
  if (observedCounter < this->deadlineCounter_)
  {
    return false;
  }
  dispatchElapsedSeconds = this->everyTickElapsedSeconds_;
  this->everyTickElapsedSeconds_ = 0.0;
  this->deadlineCounter_ = AddCounterInterval(observedCounter, counterFrequency, this->policy_.intervalSeconds);
  return true;
}

void loka::platform::Win32IdlePacer::wait(const loka::app::IdlePolicy &policy,
                                          bool idleDispatched,
                                          LONGLONG observedCounter,
                                          LONGLONG counterFrequency)
{
  if (policy.mode == loka::app::IDLE_MODE_NONE)
  {
    this->reset();
    return;
  }
  this->synchronizePolicy(policy, observedCounter, counterFrequency);
  if (policy.mode == loka::app::IDLE_MODE_INTERVAL && idleDispatched)
  {
    this->deadlineCounter_ = AddCounterInterval(observedCounter, counterFrequency, this->policy_.intervalSeconds);
  }
  this->waitUntil(this->deadlineCounter_, counterFrequency);
}

void loka::platform::Win32IdlePacer::waitUntil(LONGLONG deadlineCounter, LONGLONG counterFrequency)
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
