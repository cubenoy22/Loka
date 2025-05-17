#ifndef DECLARA_SCENE_TESTS_HPP
#define DECLARA_SCENE_TESTS_HPP

#include <cassert>
#include <stdio.h> // printfのために必要
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/Scene.hpp"
#include "core/SceneNodeGroup.hpp"
#include "core/SceneContext.hpp"
#include "core/LayoutSceneNode.hpp"
#include "app/Button.hpp"
#include "core/components/logic/format.hpp"

// --- SceneNode/SceneNodeGroup/IncrementNode/FormSceneの基本動作テスト ---
namespace SceneTests
{

  // テスト用ダミーSceneContext
  class DummySceneContext : public SceneContext
  {
  public:
    DummySceneContext() {}
    void update() {}
    SceneNodeContext *getNodeContext(SceneNode *node) { return NULL; } // 必須メソッドを実装
  };

  // --- IncrementNode: trigger発火時にcountを+1するロジック専用SceneNode（EmitterState対応） ---
  class IncrementNode : public SceneNode
  {
  public:
    IncrementNode(MutableState<int> *count, EmitterState *trigger, StateTracker *tracker)
        : SceneNode(Reuse_Singleton), count_(count), trigger_(trigger), tracker_(tracker)
    {
      assert(tracker_ && "StateTracker must not be null");
      if (trigger_)
      {
        trigger_->deferBind(&IncrementNode::onTrigger, this);
      }
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

  // --- テスト用FormScene: シンプルなカウンター ---
  class FormScene : public Scene
  {
  public:
    FormScene(PlatformContext *platform)
        : Scene(new SceneHost()),
          count(0),
          tracker(makeStateVector(&count, 0)),
          context_(platform ? platform->createSceneContextForScene(this) : NULL),
          countStr(new StrFormatState<int>(&count, "Count: %d"))
    {
    }

    ~FormScene()
    {
      delete countStr; // リソース解放
    }

    void compose(SceneNodeGroup &group)
    {
      // ※注意: ここでnewしたノードは必ずgroup.add()しないとリークするので要注意！
      SceneNodeButton *btn = new SceneNodeButton();
      btn->setText("Increment");
      group.add(new IncrementNode(&count, &btn->clickEvent, &tracker));
      group.add(
          (new LayoutSceneNode())
              ->addChild(new SceneNodeText(countStr))
              ->addChild(btn));
    }

    MutableState<int> count;
    PushStateTracker tracker;
    SceneContext *context_; // テストではNULLになることも許容
    StrFormatState<int> *countStr;
  };

  void test_IncrementNode_basic()
  {
    MutableState<int> count(0);
    EmitterState trigger;
    PushStateTracker tracker(makeStateVector(&count, 0));
    IncrementNode node(&count, &trigger, &tracker);

    // 1回発火
    trigger.emit();
    // updateはbindDefer型にリファクタ済みなら不要
    assert(count.get() == 1);

    // もう1回発火
    trigger.emit();
    assert(count.get() == 2);
  }

  void test_FormScene_compose()
  {
    // PlatformContextは不要、SceneNodeGroup単体でcomposeをテスト
    FormScene scene(NULL);
    SceneNodeGroup group;
    scene.compose(group);
    // ボタンとIncrementNodeがgroupにaddされているか
    assert(group.size() == 2);
  }

  // テストエントリーポイント
  void runAll() // runAllTestsからrunAllに名前変更
  {
    test_IncrementNode_basic();
    test_FormScene_compose();
    printf("SceneTests: All tests passed!\n");
  }
}

#endif // DECLARA_SCENE_TESTS_HPP
