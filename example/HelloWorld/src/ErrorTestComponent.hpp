#ifndef DECLARA_HELLOWORLD_ERROR_TEST_COMPONENT_HPP
#define DECLARA_HELLOWORLD_ERROR_TEST_COMPONENT_HPP

#include <string>
#include "core/State.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app/MsgBox.hpp"

namespace helloworld
{
  struct ErrorTestProps
  {
    MutableState<std::string> *title;
    MutableState<std::string> *body;
    MutableState<bool> *show;
    EmitterState *trigger;
    ErrorTestProps()
        : title(0), body(0), show(0), trigger(0) {}

    ErrorTestProps &setTitle(MutableState<std::string> *t)
    {
      title = t;
      return *this;
    }
    ErrorTestProps &setBody(MutableState<std::string> *b)
    {
      body = b;
      return *this;
    }
    ErrorTestProps &setShow(MutableState<bool> *s)
    {
      show = s;
      return *this;
    }
    ErrorTestProps &setTrigger(EmitterState *e)
    {
      trigger = e;
      return *this;
    }
  };

  inline void ErrorTestTrigger(void *userData)
  {
    ErrorTestProps *p = static_cast<ErrorTestProps *>(userData);
    if (!p)
      return;
    if (p->title)
      p->title->set("Error");
    if (p->body)
      p->body->set("This is a sample error dialog.");
    if (p->show)
      p->show->set(true, true);
  }

  inline void ErrorTestShowChanged(void *userData)
  {
    ErrorTestProps *p = static_cast<ErrorTestProps *>(userData);
    if (!p || !p->show || !p->show->get())
      return;
    declara::app::MsgBoxProps props;
    props.setTitle(p->title).setBody(p->body).setShow(p->show);
    declara::app::MsgBoxShow(NULL, props);
  }

  inline void ErrorTestSetup(const ErrorTestProps &props)
  {
    if (props.trigger)
    {
      props.trigger->bind(&ErrorTestTrigger, new ErrorTestProps(props), false);
    }
    if (props.show)
    {
      props.show->bind(&ErrorTestShowChanged, new ErrorTestProps(props), false);
    }
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_ERROR_TEST_COMPONENT_HPP
