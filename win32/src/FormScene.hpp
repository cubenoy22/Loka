#ifndef DECLARA_FORM_SCENE_HPP
#define DECLARA_FORM_SCENE_HPP

#include "core/Scene.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/PlatformContext.hpp"
#include "core/components/logic/format.hpp"
#include "core/util/StateUtil.hpp"
#include "core/LayoutSceneNode.hpp"
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
        context_(platform ? platform->createSceneContextForScene(this) : nullptr)
  {
  }

  void compose(SceneNodeGroup &group) override
  {
    ScopedAttach _(AttachTarget::Group, &group);
    SceneNodeButton *btn = NodeAs(ButtonDefinition(ButtonProps().setText("Increment")));
    Node(IncrementNodeDefinition(IncrementNodeProps(&props.count, &btn->clickEvent, &tracker)));
    LayoutSceneNode *layout = NodeAs(LayoutSceneNodeDefinition(LayoutSceneNodeProps()));
    {
      ScopedAttach _(AttachTarget::Layout, &layout);
      SceneNodeText *textNode = NodeAs(TextDefinition(TextProps().setText(static_cast<State<std::string> *>(props.countStr))), false);
      layout->addChild(btn);
    }
  }

private:
  SceneNodeGroup group_;
};

class MyAppConfig : public AppConfigurable
{
public:
  MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx) {}
  void configure(AppBuilder &builder)
  {
    builder.Window(
        new FormScene(getPlatformContext()),
        WindowOptions()
            .setTitle("DEVELOPERS!")
            .setVisibility(true));
  }
};

#endif // DECLARA_FORM_SCENE_HPP
