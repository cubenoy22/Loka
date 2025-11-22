#ifndef DECLARA_HELLOWORLD_ERROR_TEST_COMPONENT_HPP
#define DECLARA_HELLOWORLD_ERROR_TEST_COMPONENT_HPP

#include <string>
#include "core/State.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app/Button.hpp"
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

  inline declara::core::scene::NodeDefinitionBase *ErrorTest(declara::core::scene::NodeComposition &c, const ErrorTestProps &props)
  {
    using namespace declara::app;

    // Bind trigger
    if (props.trigger)
    {
      props.trigger->bind(&ErrorTestTrigger, new ErrorTestProps(props), false);
    }

    // Bind show flag to display dialog
    if (props.show)
    {
      props.show->bind(&ErrorTestShowChanged, new ErrorTestProps(props), false);
    }

    return c.declare(Button(ButtonProps().setText("Show Error").setOnClick(props.trigger)));
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_ERROR_TEST_COMPONENT_HPP
