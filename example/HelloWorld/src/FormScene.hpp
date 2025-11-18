#ifndef DECLARA_FORM_SCENE_HPP
#define DECLARA_FORM_SCENE_HPP

#include "core/State.hpp"
#include <string>
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app2/Button.hpp"
#include "app2/Box.hpp"
#include "app2/Empty.hpp"
#include "app2/Fragment.hpp"
#include "IncrementLogic.hpp"

struct FormSceneProps
{
  MutableState<std::string> buttonLabel;
};

class FormScene : public declara::core::scene::Scene
{
public:
  FormSceneProps props;
  EmitterState onClickState;

  FormScene()
      : Scene(), props()
  {
    props.buttonLabel.set("Click me!");
  }

  virtual void compose(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;

    c.declare(
        Box() << c.group(
            F() << IncrementLogic(
                      IncrementLogicProps(&props.buttonLabel, &onClickState))
                << Button(
                       ButtonProps()
                           .setText(&props.buttonLabel)
                           .setOnClick(&onClickState))));
  }
};

#endif // DECLARA_FORM_SCENE_HPP
