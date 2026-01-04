#ifndef LOKA_APP_MSGBOX_HPP
#define LOKA_APP_MSGBOX_HPP

#include "core/State.hpp"
#include "loka/core/String.hpp"

namespace declara
{
  namespace app
  {
    struct MsgBoxIcon
    {
      enum Value
      {
        Error,
        Warning,
        Information,
        Question,
        None
      };
    };

    struct MsgBoxProps
    {
      State<loka::core::String> *title;
      State<loka::core::String> *body;
      MutableState<bool> *show;
      MutableState<int> *result; // optional
      MsgBoxIcon::Value icon;
      MsgBoxProps()
          : title(0), body(0), show(0), result(0), icon(MsgBoxIcon::Error)
      {
      }
      MsgBoxProps &setTitle(State<loka::core::String> *t)
      {
        title = t;
        return *this;
      }
      MsgBoxProps &setBody(State<loka::core::String> *b)
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
      MsgBoxProps &setIcon(MsgBoxIcon::Value i)
      {
        icon = i;
        return *this;
      }
    };

    // Headless helper: when show->get() is true, shows platform dialog and resets show to false.
    // Platform-specific implementations should live under each platform directory.
    void MsgBoxShow(void *nativeWindowHandle, const MsgBoxProps &props);
  } // namespace app
} // namespace declara

#endif // LOKA_APP_MSGBOX_HPP
