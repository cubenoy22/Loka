#include "Win32App.hpp"
#include "Win32Window.hpp"
#include <windows.h>
#include <commdlg.h>
#include "app/core/App.hpp"
#include "platform/Win32IdlePacer.hpp"

Win32App::Win32App(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow)
    : App(config),
      hInstance_(hInstance),
      nCmdShow_(nCmdShow)
{
  // App already owns the shared configuration state.
}

Win32App::~Win32App()
{
  this->retireComponents();
}

void Win32App::quit()
{
  PostQuitMessage(0);
}

void Win32App::run()
{
  App::run();

  // Give each Win32 window a back-reference for native callbacks.
  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      Window *w = (*it)->asWindow();
      Win32Window *win32Win = w ? w->asWin32Window() : 0;
      if (win32Win)
      {
        win32Win->setApp(this);
      }
    }
  }

  LARGE_INTEGER frequency;
  LARGE_INTEGER lastTick;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&lastTick);
  loka::platform::Win32IdlePacer idlePacer;

  bool running = true;
  // The run loop is the clock regime: a joining write without an Operation
  // turn is a defect here and a legacy fallback everywhere else (#1057 C5).
  loka::core::Operation::Regime regime;
  while (running)
  {
    bool handledMessage = false;
    bool idleDispatched = false;
    LARGE_INTEGER now;
    loka::app::IdlePolicy waitPolicy;
    {
      loka::core::Operation turn;
      MSG msg;
      while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
      {
        handledMessage = true;
        if (msg.message == WM_QUIT)
        {
          running = false;
          break;
        }
        HWND root = msg.hwnd ? GetAncestor(msg.hwnd, GA_ROOT) : NULL;
        // Dialog navigation still reaches the outer admission/completion tail.
        if (root && IsDialogMessageW(root, &msg))
        {
          continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      if (!running)
      {
        break;
      }

      const loka::app::IdlePolicy policy = this->idlePolicy();

      QueryPerformanceCounter(&now);
      double elapsedSeconds = 0.0;
      if (frequency.QuadPart > 0)
      {
        elapsedSeconds = static_cast<double>(now.QuadPart - lastTick.QuadPart) / static_cast<double>(frequency.QuadPart);
      }
      lastTick = now;

      if (policy.mode == loka::app::IDLE_MODE_NONE)
      {
        idlePacer.reset();
        this->flushIterationTail(turn);
        if (this->hasPendingWindowAdmission())
          continue;
        waitPolicy = policy;
      }
      else
      {
        double candidateElapsedSeconds = 0.0;
        const bool idleCandidate = this->consumeIdle(elapsedSeconds, candidateElapsedSeconds);
        double dispatchElapsedSeconds = candidateElapsedSeconds;
        idleDispatched = idleCandidate;
        if (policy.mode == loka::app::IDLE_MODE_EVERY_TICK)
        {
          idleDispatched = idleCandidate
                           && idlePacer.gateEveryTick(
                               candidateElapsedSeconds, policy, now.QuadPart, frequency.QuadPart, dispatchElapsedSeconds);
        }
        if (idleDispatched)
        {
          this->handleIdle(dispatchElapsedSeconds);
        }
        this->flushIterationTail(turn);
        waitPolicy = this->idlePolicy();
        if (waitPolicy.mode == loka::app::IDLE_MODE_NONE)
        {
          idlePacer.reset();
          continue;
        }
        if (this->hasPendingWindowAdmission())
          continue;
      }
    } // Close and destroy the turn before either wait.
    if (waitPolicy.mode == loka::app::IDLE_MODE_NONE)
    {
      if (!handledMessage)
      {
#ifdef LOKA_LIFECYCLE_AUDIT
        assert(!loka::core::Operation::hasActive());
#endif
        WaitMessage();
      }
      continue;
    }
#ifdef LOKA_LIFECYCLE_AUDIT
    assert(!loka::core::Operation::hasActive());
#endif
    idlePacer.wait(waitPolicy, idleDispatched, now.QuadPart, frequency.QuadPart);
  }
}

void Win32App::flushIterationTail(loka::core::Operation &turn)
{
  this->flushMenuInvalidation();
  turn.settle();
  this->admitAndApplyWindows();
  this->reconcileFocus();
  this->admitAndApplyWindows();
  turn.close();
  this->reclaimWindows();
}

bool Win32App::handleMenuCommand(int commandId, Window *window)
{
  // Lookup and emission belong to the Window attachment. Only the legacy
  // synchronous REBUILD_MENU continuation remains here until N2a.
  (void)commandId;
  (void)window;
  this->invalidateMenu();
  return true;
}

void Win32App::applyMenuBar(Window *activeWindow)
{
  Win32Window *win = activeWindow ? activeWindow->asWin32Window() : 0;
  if (!win || !win->hwnd())
    return;
  const loka::app::MenuBarDefinition *bar = this->resolveMenuBar(activeWindow);
  // A refused native swap keeps the pending diff so the next apply retries.
  if (win->menuAttachment().project(bar, 0) != Win32MenuAttachment::PROJECT_REFUSED)
    this->clearMenuDiff();
}
