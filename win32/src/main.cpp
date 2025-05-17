#include <windows.h>
#include <string>
#include <iostream>
#include <cassert>
#include "Win32App.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/Scene.hpp"
#include "app/Button.hpp"
#include "core/PlatformContext.hpp"
#include "Win32PlatformContext.hpp"
#include "core/App.hpp"
#include "core/Window.hpp"
#include "core/SceneManager2.hpp"
#include "core/util/ScopedPtr.hpp"
#include "core/AppConfigurable.hpp"
#include "core/util/AutoTransactionGuard.hpp"
#include "core/util/StateUtil.hpp"
#include "core/LayoutSceneNode.hpp"
#include "core/components/logic/format.hpp"
#include "Tests.hpp"
#include "SceneTests.hpp"

// --- IncrementNode: trigger発火時にcountを+1するロジック専用SceneNode（EmitterState対応） ---
class IncrementNode : public SceneNode
{
public:
  IncrementNode(MutableState<int> *count, EmitterState *trigger, StateTracker *tracker)
      : SceneNode(Reuse_Singleton), count_(count), trigger_(trigger), tracker_(tracker)
  {
    assert(tracker_ && "StateTracker must not be null");
    if (trigger_)
      trigger_->deferBind(&IncrementNode::onTrigger, this);
  }

  static void onTrigger(void *userData)
  {
    IncrementNode *self = static_cast<IncrementNode *>(userData);
    AutoTransactionGuard _(self->tracker_);
    self->count_->set(self->count_->get() + 1);
  }

private:
  MutableState<int> *count_;
  EmitterState *trigger_;
  StateTracker *tracker_;
};

// --- FormScene: シンプルなカウンター ---
class FormScene : public Scene
{
public:
  FormScene(PlatformContext *platform)
      : Scene(new SceneHost()),
        count(0),
        tracker(makeStateVector(&count, 0)),
        context_(platform ? platform->createSceneContextForScene(this) : 0),
        countStr(new StrFormatState<int>(&count, "Count: %d"))
  {
    s_instance = this;
  }

  void compose(SceneNodeGroup &group)
  {
    SceneNodeAttachScope _(AttachTarget::Group, &group);
    SceneNodeButton *btn = new SceneNodeButton();
    btn->setText("Increment");
    new IncrementNode(&count, &btn->clickEvent, &tracker);

    LayoutSceneNode *layout = new LayoutSceneNode();
    {
      SceneNodeAttachScope _(AttachTarget::Layout, layout);
      new SceneNodeText(countStr);
      layout->addChild(btn);
    }
  }

  static FormScene *s_instance;
  MutableState<int> count;
  PushStateTracker tracker;
  SceneContext *context_;
  StrFormatState<int> *countStr;
};
FormScene *FormScene::s_instance = 0;

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  Win32PlatformContext platformContext;
  MyAppConfig config(&platformContext);
  ScopedPtr<App>(platformContext.createApp(&config, hInstance, nCmdShow))
      ->run();
  return 0;
}

int main()
{
  testDependencyPropagationCases();
  testTrackerPropagation();
  testDeferredSideEffect();
  testBatchTransaction();
  testRAIITransaction();
  testTextInputOnChange();
  testDerivedStruct();
  testSceneManagerTransaction();
  SceneTests::runAll();
  return 0;
}
