#ifndef DECLARA_FORM_SCENE_HPP
#define DECLARA_FORM_SCENE_HPP

#include "core/State.hpp"
#include <string>
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app2/Button.hpp"
#include "app2/Empty.hpp"
#include "app2/Fragment.hpp"
#include "app2/RowColumn.hpp"
#include "IncrementLogic.hpp"

struct ButtonLogicProps
{
  MutableState<std::string> label;
  EmitterState onClick;
};

struct FormSceneProps
{
  ButtonLogicProps buttons[4];
};

class FormScene : public declara::core::scene::Scene
{
public:
  FormSceneProps props;

  FormScene()
      : Scene(), props()
  {
    for (int i = 0; i < 4; ++i)
    {
      std::string label("Button ");
      label.push_back(static_cast<char>('1' + i));
      props.buttons[i].label.set(label);
    }
  }

  virtual void compose(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;

    c.declare(
        VStack()
        << c.group(
               HStack() << createButton(c, props.buttons[0])
                        << createButton(c, props.buttons[1]))
        << c.group(
               HStack() << createButton(c, props.buttons[2])
                        << createButton(c, props.buttons[3])));
  }

private:
  declara::core::scene::NodeDefinitionBase *createButton(declara::core::scene::NodeComposition &c, ButtonLogicProps &logicProps)
  {
    using namespace declara::app;
    return c.group(
        F() << IncrementLogic(IncrementLogicProps(&logicProps.label, &logicProps.onClick))
            << Button(ButtonProps().setText(&logicProps.label).setOnClick(&logicProps.onClick)));
  }
};

#endif // DECLARA_FORM_SCENE_HPP
