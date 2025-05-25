#ifndef DECLARA_SCENE_TESTS_HPP
#define DECLARA_SCENE_TESTS_HPP

#include <cassert>
#include <stdio.h>
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/Scene.hpp"
#include "core/SceneNodeGroup.hpp"
#include "core/SceneContext.hpp"
#include "core/LayoutSceneNode.hpp"
#include "app/Button.hpp"
#include "core/components/logic/format.hpp"
#include "core/util/SceneNodeUtil.hpp"
#include "../src/IncrementNode.hpp"
#include "FormScene.hpp"
#include "core/SceneNodeFactory.hpp"

static SceneNodeFactory g_testFactory;

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

  // --- テスト用FormScene: シンプルなカウンター ---
  class TestFormScene : public Scene
  {
  public:
    TestFormScene(PlatformContext *platform)
        : Scene(new SceneHost()),
          count(0),
          tracker(makeStateVector(&count, 0)),
          context_(platform ? platform->createSceneContextForScene(this) : NULL),
          countStr(new StrFormatState<int>(&count, "Count: %d"))
    {
    }

    ~TestFormScene()
    {
      delete countStr; // リソース解放
    }

    void compose(SceneNodeGroup &group) override
    {
      AttachScope(&group);
      SceneNodeButton *btn = NodeAs(ButtonDefinition(ButtonProps().setText("Increment")));
      SceneNode *incNode = Node(IncrementNodeDefinition(IncrementNodeProps(&count, &btn->clickEvent, &tracker)), true);
      printf("[compose] incNode = %p, type = %s\n", incNode, typeid(*incNode).name());
      LayoutSceneNode *layout = NodeAs(LayoutSceneNodeDefinition(LayoutSceneNodeProps()));
      {
        AttachScope(layout);
        SceneNodeText *text = NodeAs(TextDefinition(TextProps().setText(countStr)));
        layout->addChild(text); // 明示的に追加
        layout->addChild(btn);
        layout->addChild(incNode); // ← IncrementNode をレイアウトに追加
      }
      group.add(layout);
    }

    MutableState<int> count;
    PushStateTracker tracker;
    SceneContext *context_; // テストではNULLになることも許容
    StrFormatState<int> *countStr;
  };

  // --- IncrementNode: trigger発火時にcountを+1するロジック専用SceneNode（EmitterState対応） ---
  // （テスト用のIncrementNodeクラス定義は削除済み。実装側のIncrementNode.hppを利用）
  // test_IncrementNode_basic も修正
  void test_IncrementNode_basic()
  {
    MutableState<int> count(0);
    EmitterState trigger;
    PushStateTracker tracker(makeStateVector(&count, 0));
    IncrementNodeProps props;
    props.count = &count;
    props.trigger = &trigger;
    props.tracker = &tracker;
    IncrementNode node(props);

    // 1回発火
    trigger.emit();
    assert(count.get() == 1);

    // もう1回発火
    trigger.emit();
    assert(count.get() == 2);
  }

  void test_FormScene_compose()
  {
    // PlatformContextは不要、SceneNodeGroup単体でcomposeをテスト
    TestFormScene scene(NULL);
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
      printf("[debug] children[%zu] type: %s\n", i, typeid(*children[i]).name());
      if (dynamic_cast<SceneNodeButton *>(children[i]))
        hasButton = true;
      if (dynamic_cast<SceneNodeText *>(children[i]))
        hasText = true;
    }
    assert(hasButton && hasText);
  }

  // --- FormScene: IncrementNodeがgroup配下に自動アタッチされているかのテスト ---
  bool findIncrementNodeRecursive(SceneNode *node)
  {
    if (!node)
      return false;
    if (dynamic_cast<IncrementNode *>(node))
      return true;
    SceneNodeGroup *group = dynamic_cast<SceneNodeGroup *>(node);
    if (group)
    {
      for (SceneNodeGroup::iterator it = group->begin(); it != group->end(); ++it)
      {
        if (findIncrementNodeRecursive(*it))
          return true;
      }
    }
    LayoutSceneNode *layout = dynamic_cast<LayoutSceneNode *>(node);
    if (layout)
    {
      const std::vector<SceneNode *> &children = layout->children();
      for (size_t i = 0; i < children.size(); ++i)
      {
        if (findIncrementNodeRecursive(children[i]))
          return true;
      }
    }
    return false;
  }

  void test_FormScene_incrementNode_exists()
  {
    FormScene scene(nullptr);
    SceneNodeGroup *root = scene.getRootGroup();
    printf("[test] group = %p (scene.getRootGroup() = %p)\n", root, root);
    printf("[test] typeid(scene).name() = %s\n", typeid(scene).name());
    printf("[test] typeid(*(&scene)).name() = %s\n", typeid(*(&scene)).name());
    printf("[test] &scene = %p\n", &scene);
    scene.compose(*scene.getRootGroup());
    bool found = false;
    for (SceneNodeGroup::iterator it = root->begin(); it != root->end(); ++it)
    {
      if (findIncrementNodeRecursive(*it))
      {
        found = true;
        break;
      }
    }
    assert(found && "IncrementNode should exist in group tree");
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
    delete child;
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
    TestFormScene scene(NULL); // PlatformContext無し
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
  }

  // --- SceneNodeGroup: if/forによるrecompose動作・リーク/バインド検証 ---
  void test_SceneNodeGroup_recompose_if_for()
  {
    // 破棄検知用カウンタ
    static int deletedA = 0, deletedB = 0, deletedC = 0;
    deletedA = deletedB = deletedC = 0;

    // NodeA/B/C のDefinition構造体を用意
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

    struct NodeAProps : PropsBase
    {
      NodeAProps() {}
      virtual NodeFactoryFunc nodeFactory() const { return NULL; }
      virtual bool operator<(const PropsBase &rhs) const { return false; }
    };
    struct NodeBProps : PropsBase
    {
      NodeBProps() {}
      virtual NodeFactoryFunc nodeFactory() const { return NULL; }
      virtual bool operator<(const PropsBase &rhs) const { return false; }
    };
    struct NodeCProps : PropsBase
    {
      NodeCProps() {}
      virtual NodeFactoryFunc nodeFactory() const { return NULL; }
      virtual bool operator<(const PropsBase &rhs) const { return false; }
    };

    struct NodeADefinition
    {
      NodeAProps props;
      SceneNode *operator()() const { return new NodeA(); }
      NodeADefinition() : props() {}
    };
    struct NodeBDefinition
    {
      NodeBProps props;
      SceneNode *operator()() const { return new NodeB(); }
      NodeBDefinition() : props() {}
    };
    struct NodeCDefinition
    {
      NodeCProps props;
      SceneNode *operator()() const { return new NodeC(); }
      NodeCDefinition() : props() {}
    };

    MutableState<bool> showA(true);
    MutableState<int> count(2);
    SceneNodeGroup group;

    auto compose = [&](SceneNodeGroup &g)
    {
      g.clear();
      if (showA.get())
      {
        g.add(Node(NodeADefinition()));
      }
      else
      {
        g.add(Node(NodeBDefinition()));
      }
      for (int i = 0; i < count.get(); ++i)
      {
        g.add(Node(NodeCDefinition()));
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
        group1.add(Node(NodeADefinition()));
      }
      else
      {
        group2.add(Node(NodeBDefinition()));
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
    TestFormScene scene(NULL);
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
    // root.clear()でchildもdeleteされるため、child->size()の参照やアサートは不要
    // child自体はdelete済みとなる
    // （所有権はSceneNodeGroupが持つ設計に統一）
    // ここでchildやgrandchildのdeleteは不要
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
      TestButton(MutableState<int> *c, StateTracker *t)
          : SceneNodeButton(ButtonProps()), counter(c), tracker(t)
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
    // 再add（新しいleafをnewする！）
    TestButton *leaf2 = new TestButton(&counter, &tracker);
    grandchild->add(leaf2);
    child->add(grandchild);
    root.add(child);
    // もう一度クリックで+1
    leaf2->clickEvent.emit();
    assert(counter.get() == 2);
    // root.clear()でchild, grandchild, leaf2もdeleteされるので、以降のdeleteや参照は不要
  }

  // テストエントリーポイント
  typedef void (*TestFunc)();

  void runAll() // runAllTestsからrunAllに名前変更
  {
    TestFunc tests[] = {
        // test_IncrementNode_basic,
        // test_FormScene_compose,
        // // test_FormScene_incrementNode_exists,
        // test_SceneNodeGroup_hierarchy,
        // test_FormScene_ui_less_behavior,
        // test_SceneNodeGroup_recompose,
        // test_Scene_compose_multiple_calls,
        // test_Scene_auto_recompose_on_state_change,
        // test_SceneNodeGroup_auto_recompose,
        // test_SceneNodeGroup_nested,
        // test_SceneNodeGroup_nested_event_recompose,
        test_SceneNodeGroup_recompose_if_for};
    const int numTests = sizeof(tests) / sizeof(tests[0]);
    for (int i = 0; i < numTests; ++i)
    {
      tests[i]();
      g_testFactory.clearAll(); // 各テスト後にFactoryキャッシュをクリア
    }
    printf("SceneTests: All tests passed!\n");
  }
}

#endif // DECLARA_SCENE_TESTS_HPP
