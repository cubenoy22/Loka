#include <windows.h>
#include "app/MsgBox.hpp"

namespace declara
{
  namespace app
  {
    void MsgBoxShow(void *nativeWindowHandle, const MsgBoxProps &props)
    {
      if (!props.show || !props.title || !props.body)
        return;
      if (!props.show->get())
        return;

      UINT iconFlag = 0;
      switch (props.icon)
      {
      case MsgBoxIcon::Error:
        iconFlag = MB_ICONERROR;
        break;
      case MsgBoxIcon::Warning:
        iconFlag = MB_ICONWARNING;
        break;
      case MsgBoxIcon::Information:
        iconFlag = MB_ICONINFORMATION;
        break;
      case MsgBoxIcon::Question:
        iconFlag = MB_ICONQUESTION;
        break;
      case MsgBoxIcon::None:
      default:
        iconFlag = 0;
        break;
      }

      const std::string &t = props.title->get();
      const std::string &b = props.body->get();
      HWND hwnd = reinterpret_cast<HWND>(nativeWindowHandle);
      int res = MessageBoxA(hwnd ? hwnd : GetActiveWindow(), b.c_str(), t.c_str(), MB_OK | iconFlag);
      if (props.result)
      {
        props.result->set(res, true);
      }
      props.show->set(false, true);
    }
  } // namespace app
} // namespace declara

