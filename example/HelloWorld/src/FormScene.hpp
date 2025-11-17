#ifndef DECLARA_FORM_SCENE_HPP
#define DECLARA_FORM_SCENE_HPP

#include "core/State.hpp"
#include <cstdio>
#include <string>
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app2/Button.hpp"
#include "app2/Box.hpp"
#include "app2/Empty.hpp"

// Forward declaration
class FormScene;

// Global function definition
static void FormScene_onButtonClicked(void *userData);

struct FormSceneProps
{
  MutableState<int> count;
  MutableState<std::string> buttonLabel;
};

class FormScene : public declara::core::scene::Scene
{
public:
  FormSceneProps props;
  EmitterState onClickState;
  int clickCount_;

  FormScene()
      : Scene(), props(), clickCount_(0)
  {
    props.buttonLabel.set(std::string("Click me"), true);
    onClickState.deferBind(&FormScene_onButtonClicked, this);
  }

  virtual void compose(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;

    Box root;
    root << *c.store(
        Button(
            ButtonProps()
                .setText(&props.buttonLabel)
                .setOnClick(&onClickState)));
    c.declare(root);
  }

  friend void FormScene_onButtonClicked(void *userData);
};

// Implementation
static void FormScene_onButtonClicked(void *userData)
{
  FormScene *self = static_cast<FormScene *>(userData);
  if (!self)
    return;
  ++self->clickCount_;
  char buf[64];
  sprintf(buf, "Clicked %d", self->clickCount_);
  self->props.buttonLabel.set(std::string(buf), true);
}

#endif // DECLARA_FORM_SCENE_HPP
