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
#include "Tests.hpp"

// --- FormScene: シンプルなカウンター ---
class FormScene : public Scene
{
public:
  FormScene(PlatformContext *platform)
      : Scene(new SceneHost()),
        count(0),
        tracker(makeStateVector(&count, 0)),
        context_(platform ? platform->createSceneContextForScene(this) : nullptr)
  {
    s_instance = this;
  }
  void compose(SceneNodeGroup &group)
  {
    LayoutSceneNode *layout = new LayoutSceneNode();
    // カウント表示
    layout->addChild(new SceneNodeText(std::string("Count: ") + std::to_string(count.get())));
    // インクリメントボタン
    layout->addChild(
        (new SceneNodeButton(ButtonProps()
                                 .setText("Increment")
                                 .setOnClick(&FormScene::onIncrementClicked))));
    group.add(layout);
  }
  static void onIncrementClicked()
  {
    if (s_instance)
      s_instance->count.set(s_instance->count.get() + 1);
  }
  static FormScene *s_instance;
  MutableState<int> count;
  PushStateTracker tracker;
  SceneContext *context_;
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
  return 0;
}
