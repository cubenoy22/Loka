#ifndef LOKA_WIN32_FOCUS_PARTICIPANT_HPP
#define LOKA_WIN32_FOCUS_PARTICIPANT_HPP

#include <windows.h>
#include "app/scene/Node.hpp"

/** Typed, non-owning HWND property shared only by focus participants.
    SetPropW interns the name as a property atom. Store the adjusted NodeContext
    pointer, never GWLP_USERDATA. Attach sets it; detach removes it synchronously
    at lifecycle fact delivery. A refused SetPropW leaves the HWND unmarked;
    the next attach retries. This property never owns the context. */
class Win32FocusParticipant
{
public:
  static bool attach(HWND hwnd, loka::app::scene::NodeContext *context)
  {
    return SetPropW(hwnd, name(), context) != FALSE;
  }
  static void detach(HWND hwnd)
  {
    RemovePropW(hwnd, name());
  }
  static loka::app::scene::NodeContext *read(HWND hwnd)
  {
    return static_cast<loka::app::scene::NodeContext *>(GetPropW(hwnd, name()));
  }

private:
  static LPCWSTR name() { return L"Loka.FocusParticipant.NodeContext"; }
};

#endif
