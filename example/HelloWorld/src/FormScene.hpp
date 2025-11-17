#ifndef DECLARA_FORM_SCENE_HPP
#define DECLARA_FORM_SCENE_HPP

#include "core/State.hpp"
#include <string>
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app2/Button.hpp"
#include "app2/Box.hpp"
#include "app2/Empty.hpp"
#include "app2/logic/IncrementLogic.hpp"

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

    Box root;
    root << *c.store(
        IncrementLogic(
            IncrementLogicProps()
                .setLabel(&props.buttonLabel)
                .setTrigger(&onClickState)));
    root << *c.store(
        Button(
            ButtonProps()
                .setText(&props.buttonLabel)
                .setOnClick(&onClickState)));
    c.declare(root);
  }
};

#endif // DECLARA_FORM_SCENE_HPP
