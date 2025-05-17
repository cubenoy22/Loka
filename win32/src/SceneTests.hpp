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

  // --- SceneNodeGroup 階層構造・動的生成/破棄テスト ---
  void test_SceneNodeGroup_hierarchy()
  {
    SceneNodeGroup root;
    SceneNodeGroup *child = new SceneNodeGroup();
    SceneNodeGroup *grandchild = new SceneNodeGroup();

    // 階層構造を構築
    child->add((SceneNode *)grandchild);
    root.add((SceneNode *)child);

    // 構造確認
    assert(root.size() == 1);
    assert(child->size() == 1);
    assert(grandchild->size() == 0);

    // 動的生成・破棄
    root.remove((SceneNode *)child); // removeしてもchild自体はdeleteしない
    assert(root.size() == 0);
    delete child; // grandchildもdeleteされるべき（デストラクタで）
  }

  // --- FormScene: UIless(PlatformContext無し)でも期待通り動作するかのテスト ---
  // SceneNodeGroup/SceneNodeツリーからSceneNodeButton*を再帰的に探索するヘルパー
  SceneNodeButton *findButtonRecursive(SceneNode *node)
  {
    if (!node)
      return nullptr;
    SceneNodeButton *btn = dynamic_cast<SceneNodeButton *>(node);
    if (btn)
      return btn;
    // SceneNodeGroup/またはLayoutSceneNodeなら子ノードを探索
    SceneNodeGroup *group = dynamic_cast<SceneNodeGroup *>(node);
    if (group)
    {
      for (SceneNodeGroup::iterator it = group->begin(); it != group->end(); ++it)
      {
        btn = findButtonRecursive(*it);
        if (btn)
          return btn;
      }
    }
    LayoutSceneNode *layout = dynamic_cast<LayoutSceneNode *>(node);
    if (layout)
    {
      const std::vector<SceneNode *> &children = layout->children();
      for (size_t i = 0; i < children.size(); ++i)
      {
        btn = findButtonRecursive(children[i]);
        if (btn)
          return btn;
      }
    }
    return nullptr;
  }

  void test_FormScene_ui_less_behavior()
  {
    FormScene scene(NULL); // PlatformContext無し
    SceneNodeGroup group;
    scene.compose(group);
    // ボタンとIncrementNodeがgroupにaddされているか
    assert(group.size() == 2);
    // ボタンのclickEventを直接発火してカウントアップするか
    SceneNodeButton *btn = nullptr;
    for (SceneNodeGroup::iterator it = group.begin(); it != group.end(); ++it)
    {
      btn = findButtonRecursive(*it);
      if (btn)
        break;
    }
    assert(btn != nullptr);
    int before = scene.count.get();
    btn->clickEvent.emit();
    assert(scene.count.get() == before + 1);
  }

  // --- SceneNodeGroup: recompose（再構成）テスト ---
  void test_SceneNodeGroup_recompose()
  {
    SceneNodeGroup group;
    // 1回目: ノードを2つ追加
    SceneNode *n1 = new SceneNode();
    SceneNode *n2 = new SceneNode();
    group.add(n1);
    group.add(n2);
    assert(group.size() == 2);

    // remove/addで再構成
    group.remove(n1);
    assert(group.size() == 1);
    group.add(n1);
    assert(group.size() == 2);

    // clearして再度追加
    group.clear();
    assert(group.size() == 0);
    SceneNode *n3 = new SceneNode();
    group.add(n3);
    assert(group.size() == 1);

    // 2回目: 別のノードセットで再構成
    group.clear();
    SceneNode *n4 = new SceneNode();
    group.add(n4);
    assert(group.size() == 1);

    // 後始末
    delete n1;
    delete n2;
    delete n3;
    delete n4;
  }

  // --- Scene: compose多重呼び出しテスト ---
  void test_Scene_compose_multiple_calls()
  {
    FormScene scene(NULL);
    SceneNodeGroup group;
    // 1回目compose
    scene.compose(group);
    assert(group.size() == 2);

    // 2回目compose（groupをclearせず再度呼ぶ）
    scene.compose(group);
    // 追加実装によっては2→4になる可能性もあるが、現状はadd()で積み上がる設計なので4になるはず
    assert(group.size() == 4);

    // groupをclearしてから再compose
    group.clear();
    assert(group.size() == 0);
    scene.compose(group);
    assert(group.size() == 2);
  }

  // --- Scene: State変更で自動recompose発火テスト ---
  class TestableScene : public Scene
  {
  public:
    int composeCount;
    MutableState<int> state;
    PushStateTracker tracker;

    TestableScene() : Scene(new SceneHost()), state(0), tracker(makeStateVector(&state, 0))
    {
      composeCount = 0; // Scene基底コンストラクタ後に0でリセット
      state.bind(&TestableScene::onStateChanged, this, false);
    }
    static void onStateChanged(void *userData)
    {
      TestableScene *self = static_cast<TestableScene *>(userData);
      self->recompose();
    }
    void compose(SceneNodeGroup &group) override
    {
      composeCount++;
      group.add(new SceneNode());
    }
    void recompose()
    {
      rootGroup_->clear();
      compose(*rootGroup_);
    }
  };

  void test_Scene_auto_recompose_on_state_change()
  {
    TestableScene scene;
    assert(scene.composeCount == 0); // コンストラクタ後は0
    {
      AutoTransactionGuard _(static_cast<StateTracker *>(&scene.tracker));
      scene.state.set(42);
    }
    assert(scene.composeCount == 1); // State変更でrecomposeが1回発火
  }

  // テストエントリーポイント
  void runAll() // runAllTestsからrunAllに名前変更
  {
    test_IncrementNode_basic();
    test_FormScene_compose();
    test_SceneNodeGroup_hierarchy();
    test_FormScene_ui_less_behavior();
    test_SceneNodeGroup_recompose();
    test_Scene_compose_multiple_calls();
    test_Scene_auto_recompose_on_state_change();
    printf("SceneTests: All tests passed!\n");
  }
}

#endif // DECLARA_SCENE_TESTS_HPP
