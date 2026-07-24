#ifndef LOKA_WIN32_THREAD_MODAL_DIALOG_SCOPE_HPP
#define LOKA_WIN32_THREAD_MODAL_DIALOG_SCOPE_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>

namespace loka
{
  namespace win32
  {
    /** Common dialogs disable only their owner window; every other top-level
        window on the thread keeps accepting input inside the dialog's nested
        message loop, and a control click there can re-enter the suspended
        update cycle (#152). Hold the rest of the thread's windows disabled
        for the dialog's lifetime, restoring exactly the ones this scope
        disabled. */
    class ThreadModalDialogScope
    {
    public:
      explicit ThreadModalDialogScope(HWND owner)
          : owner_(owner),
            disabled_()
      {
        EnumThreadWindows(GetCurrentThreadId(),
                          &ThreadModalDialogScope::DisableOtherWindowThunk,
                          reinterpret_cast<LPARAM>(this));
      }

      ~ThreadModalDialogScope()
      {
        for (std::size_t i = disabled_.size(); i > 0; --i)
        {
          EnableWindow(disabled_[i - 1], TRUE);
        }
      }

    private:
      static BOOL CALLBACK DisableOtherWindowThunk(HWND hwnd, LPARAM lParam)
      {
        ThreadModalDialogScope *self = reinterpret_cast<ThreadModalDialogScope *>(lParam);
        if (hwnd != self->owner_ && IsWindowEnabled(hwnd))
        {
          EnableWindow(hwnd, FALSE);
          self->disabled_.push_back(hwnd);
        }
        return TRUE;
      }

      HWND owner_;
      std::vector<HWND> disabled_;

      ThreadModalDialogScope(const ThreadModalDialogScope &);
      ThreadModalDialogScope &operator=(const ThreadModalDialogScope &);
    };

  } // namespace win32
} // namespace loka

#endif // LOKA_WIN32_THREAD_MODAL_DIALOG_SCOPE_HPP
