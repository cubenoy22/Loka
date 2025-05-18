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

    void compose(SceneNodeGroup &group) override
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
#ifdef TEST_BUILD
      // --- デバッグ出力: groupに含まれるノードの型を表示 ---
      printf("[FormScene::compose] group.size() = %zu\n", group.size());
      int idx = 0;
      for (SceneNodeGroup::iterator it = group.begin(); it != group.end(); ++it, ++idx)
      {
        printf("  group[%d]: %s\n", idx, typeid(**it).name());
      }
#endif
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
    // groupにLayoutSceneNodeが1つ以上含まれていることを検証
    size_t layoutCount = 0;
    LayoutSceneNode *layout = nullptr;
    for (SceneNodeGroup::iterator it = group.begin(); it != group.end(); ++it)
    {
      LayoutSceneNode *candidate = dynamic_cast<LayoutSceneNode *>(*it);
      if (candidate)
      {
        layout = candidate;
        ++layoutCount;
      }
    }
    assert(layoutCount >= 1);
    assert(layout != nullptr);
    // layoutの子にSceneNodeButtonとSceneNodeTextがいるか
    bool hasButton = false, hasText = false;
    const std::vector<SceneNode *> &children = layout->children();
    for (size_t i = 0; i < children.size(); ++i)
    {
      if (dynamic_cast<SceneNodeButton *>(children[i]))
        hasButton = true;
      if (dynamic_cast<SceneNodeText *>(children[i]))
        hasText = true;
    }
    assert(hasButton && hasText);
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

  // --- SceneNodeGroup: if/forによるrecompose動作・リーク/バインド検証 ---
  void test_SceneNodeGroup_recompose_if_for()
  {
    // 破棄検知用カウンタ
    static int deletedA = 0, deletedB = 0, deletedC = 0;
    deletedA = deletedB = deletedC = 0;

    struct NodeA : SceneNode
    {
      ~NodeA() { ++deletedA; }
    };
    struct NodeB : SceneNode
    {
      ~NodeB() { ++deletedB; }
    };
    struct NodeC : SceneNode
    {
      ~NodeC() { ++deletedC; }
    };

    MutableState<bool> showA(true);
    MutableState<int> count(2);
    SceneNodeGroup group;

    auto compose = [&](SceneNodeGroup &g)
    {
      g.clear();
      if (showA.get())
      {
        g.add(new NodeA());
      }
      else
      {
        g.add(new NodeB());
      }
      for (int i = 0; i < count.get(); ++i)
      {
        g.add(new NodeC());
      }
    };

    // 1. 初期状態: showA=true, count=2
    compose(group);
    assert(group.size() == 3);

    // 2. showA=falseにしてrecompose
    showA.set(false);
    compose(group);
    assert(group.size() == 3);
    assert(deletedA == 1);
    assert(deletedB == 0);

    // 3. count=1にしてrecompose
    count.set(1);
    compose(group);
    assert(group.size() == 2);
    assert(deletedC == 1);

    // 4. showA=true, count=0にしてrecompose
    showA.set(true);
    count.set(0);
    compose(group);
    assert(group.size() == 1);
    assert(deletedA == 1);
    assert(deletedB == 1);

    // 5. 全部消す
    group.clear();
    assert(group.size() == 0);
    assert(deletedA + deletedB + deletedC == 3);

    // --- Stateを跨ぐSceneNodeGroupの動的切り替え（挑戦的） ---
    MutableState<bool> useGroup1(true);
    SceneNodeGroup group1, group2;
    auto compose2 = [&]()
    {
      group1.clear();
      group2.clear();
      if (useGroup1.get())
      {
        group1.add(new NodeA());
      }
      else
      {
        group2.add(new NodeB());
      }
    };
    compose2();
    assert(group1.size() == 1 && group2.size() == 0);
    useGroup1.set(false);
    compose2();
    assert(group1.size() == 0 && group2.size() == 1);
    useGroup1.set(true);
    compose2();
    assert(group1.size() == 1 && group2.size() == 0);
  }

  // --- Scene: compose多重呼び出しテスト ---
  void test_Scene_compose_multiple_calls()
  {
    FormScene scene(NULL);
    SceneNodeGroup group;
    // 1回目compose
    scene.compose(group);
    assert(group.size() == 1);

    // 2回目compose前にclearしてから再度呼ぶ
    group.clear();
    scene.compose(group);
    assert(group.size() == 1);

    // groupをclearしてから再compose
    group.clear();
    assert(group.size() == 0);
    scene.compose(group);
    assert(group.size() == 1);
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

  // --- SceneNodeGroup: State依存で自動recomposeテスト ---
  void test_SceneNodeGroup_auto_recompose()
  {
    MutableState<int> state(0);
    std::vector<StateBase *> deps;
    deps.push_back(&state);
    SceneNodeGroup group(deps);
    SceneNode *n1 = new SceneNode();
    group.add(n1);
    assert(group.size() == 1);

    // State変更でrecompose（clear）が自動で呼ばれる
    state.set(42);
    assert(group.size() == 0); // clearされたはず

    // 後始末
    delete n1;
  }

  // --- SceneNodeGroup: ネスト構造テスト ---
  void test_SceneNodeGroup_nested()
  {
    SceneNodeGroup root;
    SceneNodeGroup *child = new SceneNodeGroup();
    SceneNodeGroup *grandchild = new SceneNodeGroup();
    SceneNode *leaf = new SceneNode();

    // 構造: root → child → grandchild → leaf
    grandchild->add(leaf);
    child->add(grandchild);
    root.add(child);

    // 各階層のサイズを検証
    assert(root.size() == 1);
    assert(child->size() == 1);
    assert(grandchild->size() == 1);

    // grandchild から leaf を remove
    grandchild->remove(leaf);
    assert(grandchild->size() == 0);

    // child から grandchild を remove
    child->remove(grandchild);
    assert(child->size() == 0);

    // root から child を remove
    root.remove(child);
    assert(root.size() == 0);

    // 再度 add して clear で全消去
    child->add(grandchild);
    root.add(child);
    root.clear();
    assert(root.size() == 0);
    assert(child->size() == 1); // child自体は残る（rootから外れただけ）

    // 後始末
    delete leaf;
    delete grandchild;
    delete child;
  }

  // --- SceneNodeGroup: ネスト+イベント伝播+recomposeテスト ---
  void test_SceneNodeGroup_nested_event_recompose()
  {
    // rootにカウンタState
    MutableState<int> counter(0);
    SceneNodeGroup root;
    SceneNodeGroup *child = new SceneNodeGroup();
    SceneNodeGroup *grandchild = new SceneNodeGroup();

    // leaf: ボタン（クリックでcounter+1）
    class TestButton : public SceneNodeButton
    {
    public:
      MutableState<int> *counter;
      StateTracker *tracker;
      TestButton(MutableState<int> *c, StateTracker *t) : counter(c), tracker(t)
      {
        clickEvent.deferBind([](void *ud)
                             {
          auto *self = static_cast<TestButton *>(ud);
          AutoTransactionGuard _(self->tracker);
          self->counter->set(self->counter->get() + 1); }, this);
      }
    };
    PushStateTracker tracker(makeStateVector(&counter, 0));
    TestButton *leaf = new TestButton(&counter, &tracker);

    grandchild->add(leaf);
    child->add(grandchild);
    root.add(child);

    // クリック前
    assert(counter.get() == 0);
    // クリックで+1
    leaf->clickEvent.emit();
    assert(counter.get() == 1);

    // root.clear() → 再構築
    root.clear();
    assert(root.size() == 0);
    // 再add
    child->add(grandchild);
    root.add(child);
    // もう一度クリックで+1
    leaf->clickEvent.emit();
    assert(counter.get() == 2);

    // 後始末
    delete leaf;
    delete grandchild;
    delete child;
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
    test_SceneNodeGroup_auto_recompose();
    test_SceneNodeGroup_nested();
    test_SceneNodeGroup_nested_event_recompose();
    test_SceneNodeGroup_recompose_if_for(); // if/forによるrecomposeテスト
    printf("SceneTests: All tests passed!\n");
  }
}

#endif // DECLARA_SCENE_TESTS_HPP
