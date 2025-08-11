#ifndef DECLARA_FORM_SCENE_HPP
#define DECLARA_FORM_SCENE_HPP

#include "core/State.hpp"
/*
#include "core/StateTracker.hpp"
#include "core/PlatformContext.hpp"
#include "core/components/logic/format.hpp"
#include "core/util/StateUtil.hpp"
#include "app/Button.hpp"
#include "app/Text.hpp"
#include "IncrementNode.hpp"

struct FormSceneProps
{
  MutableState<int> count;
  StrFormatState<int> *countStr;
  FormSceneProps()
      : count(0),
        countStr(new StrFormatState<int>(&count, "Count: %d")) {}
  ~FormSceneProps() { delete countStr; }
};

class FormScene : public Scene
{
public:
  FormSceneProps props;
  PushStateTracker tracker;
  SceneContext *context_;

  FormScene(PlatformContext *platform)
      : Scene(new SceneHost()),
        props(),
        tracker(makeStateVector(&props.count, 0)),
        context_(platform ? platform->createSceneContextForScene(this) : nullptr) {}

  void compose(SceneNodeGroup &group) override
  {
    ScopedAttach _(AttachTarget::Group, &group);
  ButtonProps btnProps;
  btnProps.setText("Increment");
  ButtonDefinition buttonDef = ButtonDefinition(btnProps);
    SceneNodeButton *btn = NodeAs(buttonDef);
    Node(IncrementNodeDefinition(IncrementNodeProps(&props.count, &btn->clickEvent, &tracker)));
    LayoutSceneNode *layout = NodeAs(LayoutSceneNodeDefinition(LayoutSceneNodeProps()));
    {
      ScopedAttach _(AttachTarget::Layout, &layout);
      SceneNodeText *textNode = NodeAs(TextDefinition(TextProps().setText(static_cast<State<std::string> *>(props.countStr))), false);
      *layout << btn;
    }
  }

private:
  SceneNodeGroup group_;
};
*/

// --- 新namespace版: core2/scene, app2 などに切り替え ---
#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "app2/Button.hpp"
#include "app2/Box.hpp"
#include "app2/Empty.hpp"
#include "core/AppComponent.hpp"
#include "core/AppConfigurable.hpp"
// #include "app2/Text.hpp"
// #include "app2/LayoutSceneNode.hpp"
// #include "IncrementNode.hpp" // 未移植の場合はコメントアウト

struct FormSceneProps
{
  MutableState<int> count;
  // StrFormatState<int> *countStr;
};

class FormScene : public declara::core::scene::Scene
{
public:
  FormSceneProps props;
  EmitterState onClickState;
  // PushStateTracker tracker;

  FormScene(/*PlatformContext *platform*/)
      : Scene(), props() /*, tracker(...)*/ {}

  void compose(declara::core::scene::NodeComposition c) override
  {
    using declara::app::Box;
    using declara::app::Button;
    using declara::app::ButtonProps;
    // 実用的ではないサンプル: fruits配列をmapでButtonに変換しBoxに<<する
    std::vector<std::string> fruits = {"Apple", "Banana", "Cherry"};
    struct FruitToButton
    {
      typedef declara::app::ButtonDefinition result_type;
      result_type operator()(const std::string &name) const
      {
        declara::app::ButtonProps props;
        props.setText(name);
        return declara::app::ButtonDefinition(props);
      }
    };
    auto fruitButtons = c.map(c.stream(fruits), FruitToButton());
    std::vector<declara::core::scene::NodeDefinitionBase *> fruitButtonsVec;
    {
      std::vector<declara::app::ButtonDefinition> tmp = fruitButtons.toVector();
      for (size_t i = 0; i < tmp.size(); ++i)
      {
        fruitButtonsVec.push_back(&tmp[i]);
      }
    }
    Box box;
    box << fruitButtonsVec;
    box << c.conditional(StaticState(false), c.group(Button()));
    box << Button(ButtonProps().setText("").setOnClick(&onClickState));
    Box groupBox;
    groupBox << Button(ButtonProps());
    box << static_cast<declara::core::scene::NodeDefinitionBase &>(groupBox);
    box << Button();
    c.declare(box);
  }
};

class MyAppConfig : public AppConfigurable
{
public:
  MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx) {}
  void configure(AppBuilder &builder)
  {
    builder.Window(
        new declara::core::scene::Scene(),
        WindowOptions()
            .setTitle("DEVELOPERS!")
            .setVisibility(true));
  }
};

#endif // DECLARA_FORM_SCENE_HPP
