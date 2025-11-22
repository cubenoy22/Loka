#ifndef DECLARA_APP_MSGBOX_HPP
#define DECLARA_APP_MSGBOX_HPP

#include <string>
#include <windows.h>
#include "core/State.hpp"

namespace declara
{
  namespace app
  {
    struct MsgBoxProps
    {
      State<std::string> *title;
      State<std::string> *body;
      MutableState<bool> *show;
      MutableState<int> *result; // optional
      MsgBoxProps()
          : title(0), body(0), show(0), result(0)
      {
      }
      MsgBoxProps &setTitle(State<std::string> *t)
      {
        title = t;
        return *this;
      }
      MsgBoxProps &setBody(State<std::string> *b)
      {
        body = b;
        return *this;
      }
      MsgBoxProps &setShow(MutableState<bool> *s)
      {
        show = s;
        return *this;
      }
      MsgBoxProps &setResult(MutableState<int> *r)
      {
        result = r;
        return *this;
      }
    };

    // Headless helper: when show->get() is true, shows MessageBox and resets show to false.
    inline void MsgBoxShow(HWND hwnd, const MsgBoxProps &props)
    {
      if (!props.show || !props.title || !props.body)
        return;
      if (!props.show->get())
        return;

      const std::string &t = props.title->get();
      const std::string &b = props.body->get();
      int res = MessageBoxA(hwnd ? hwnd : GetActiveWindow(), b.c_str(), t.c_str(), MB_OK | MB_ICONERROR);
      if (props.result)
      {
        props.result->set(res, true);
      }
      props.show->set(false, true);
    }
  } // namespace app
} // namespace declara

#endif // DECLARA_APP_MSGBOX_HPP
