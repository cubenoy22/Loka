#include "FormScene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app2/Button.hpp"
#include "app2/Box.hpp"
#include "app2/Empty.hpp"

using namespace declara::core::scene;

void FormScene::compose(NodeComposition &c)
{
  declara::app::BoxProps boxProps;
  declara::app::BoxDefinition box(boxProps);
  declara::app::BoxDefinition &root = c.declare(box);

  declara::app::ButtonProps buttonProps;
  buttonProps.setText(static_cast<State<std::string> *>(&props.buttonLabel));
  buttonProps.setOnClick(&onClickState);
  declara::app::ButtonDefinition button(buttonProps);

  root << *c.copyToArena(button);
}
