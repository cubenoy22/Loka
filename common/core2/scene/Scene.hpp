#ifndef DECLARA_CORE2_SCENE_SCENE_HPP
#define DECLARA_CORE2_SCENE_SCENE_HPP

#include "core/State.hpp"
#include <cassert>
#include "core2/scene/Node.hpp" // NodeDefinitionBase 使用のため定義を取得

enum SceneLifecycle
{
  ON_CREATE = 0,
  ON_ATTACH = 1,
  ON_DETACH = 2
};

namespace declara
{
  namespace core
  {
    namespace scene
    {
      // 前方宣言のみ。詳細は利用側の実装ファイルでincludeする
      struct NodeComposition;

      class Scene
      {
      public:
        // ルート未指定は禁止 (C++98-compatible: declare private, no definition)
        // ルート定義ポインタを所有 (生ポインタをそのまま保持し destructor で delete)
        explicit Scene(NodeDefinitionBase *def) : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def)
        {
          assert(def && "Scene requires a root definition");
          assert(def->isBoundary() && "Scene root must be a Boundary definition");
        }
        // ルート定義を clone して所有するオーバーロード
        explicit Scene(const NodeDefinitionBase &def) : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def.clone())
        {
          assert(def.isBoundary() && "Scene root must be a Boundary definition");
        }
        virtual ~Scene()
        {
          if (rootDefinition_)
          {
            delete rootDefinition_;
            rootDefinition_ = 0;
          }
        }

        NodeDefinitionBase *getRootDefinition() const { return rootDefinition_; }

        // 外部公開: State*として取得
        State<SceneLifecycle> *getLifecycleState() { return &lifecycle_; }
        State<bool> *getAttachedState() { return &attached_; }

        // 公開ラッパ: 外部から安全にライフサイクル/アタッチ状態を更新する用途
        // (SceneManager2 以外での最小限の操作許可。副作用管理は呼び出し側で StateTracker を利用する想定)
        void updateAttached(bool v) { setAttached(v); }
        void updateLifecycle(SceneLifecycle v) { setLifecycle(v); }

      protected:
        void setAttached(bool v) { attached_.set(v, true); }
        void setLifecycle(SceneLifecycle v) { lifecycle_.set(v, true); }

        MutableState<SceneLifecycle> lifecycle_;
        MutableState<bool> attached_;
        NodeDefinitionBase *rootDefinition_;

        // SceneManager2からlifecycle_/attachedを書き換え可能に
        friend class SceneManager2;

      private:
        // Default constructor intentionally not implemented to forbid rootless scenes
        Scene();
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_SCENE_HPP
