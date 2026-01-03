#ifndef DECLARA_CORE2_SCENE_SCENE_HPP
#define DECLARA_CORE2_SCENE_SCENE_HPP

#include "core/State.hpp"
#include <cassert>
#include "core2/scene/Node.hpp" // NodeDefinitionBase 使用のため定義を取得
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/ComponentContext.hpp"
#include "core2/scene/node/Boundary.hpp"

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
        // Boundary 定義のみを受け付ける (compile-time check via IsBoundaryDefinition)
        template <class DefT>
        explicit Scene(DefT *def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), mounted_(false), composed_(false)
        {
          assert(def && "Scene requires a root definition");
        }
        // ルート定義を clone して所有するオーバーロード
        template <class DefT>
        explicit Scene(const DefT &def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def.clone()), rootNode_(0), platformController_(0), mounted_(false), composed_(false)
        {
        }
        virtual ~Scene()
        {
          unmount();
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
        void updateAttached(bool v)
        {
          setAttached(v);
          if (mounted_)
          {
            if (v)
            {
              composeIfNeeded(COMPOSE_EVENT_ATTACH);
            }
            else
            {
              notifyComposeEvent(COMPOSE_EVENT_DETACH);
              teardownComposition();
            }
          }
        }
        void updateLifecycle(SceneLifecycle v) { setLifecycle(v); }

        void mount(IPlatformController *platformController)
        {
          assert(platformController && "Scene::mount requires a platform controller");
          platformController_ = platformController;
          mounted_ = true;
          ensureRootNode();
          if (attached_.get())
          {
            composeIfNeeded(COMPOSE_EVENT_ATTACH);
          }
        }

        void unmount()
        {
          notifyComposeEvent(COMPOSE_EVENT_DETACH);
          teardownComposition();
          mounted_ = false;
          platformController_ = 0;
          if (rootNode_)
          {
            delete rootNode_;
            rootNode_ = 0;
          }
        }

      protected:
        void setAttached(bool v) { attached_.set(v, true); }
        void setLifecycle(SceneLifecycle v) { lifecycle_.set(v, true); }

        MutableState<SceneLifecycle> lifecycle_;
        MutableState<bool> attached_;
        NodeDefinitionBase *rootDefinition_;
        Node *rootNode_;
        IPlatformController *platformController_;
        bool mounted_;
        bool composed_;

        // SceneManager2からlifecycle_/attachedを書き換え可能に
        friend class SceneManager2;

      private:
        void ensureRootNode()
        {
          if (rootNode_)
          {
            return;
          }
          assert(rootDefinition_ && "Scene requires a root definition");
          rootNode_ = rootDefinition_->create();
          assert(rootNode_ && "Scene failed to create root node");
          BoundaryNode *boundary = dynamic_cast<BoundaryNode *>(rootNode_);
          assert(boundary && "Scene root must be a Boundary node");
        }

        void composeIfNeeded(ComposeEvent event)
        {
          if (composed_ || !platformController_)
          {
            return;
          }
          if (!rootNode_)
          {
            ensureRootNode();
          }
          BoundaryNode *boundary = dynamic_cast<BoundaryNode *>(rootNode_);
          assert(boundary && "Scene root must be a Boundary node");
          ComponentContext rootContext;
          boundary->compose(rootContext, event);
          platformController_->onChange(rootNode_, NODE_DIRTY_INITIAL);
          composed_ = true;
        }

        void notifyComposeEvent(ComposeEvent event)
        {
          if (!rootNode_)
          {
            return;
          }
          BoundaryNode *boundary = dynamic_cast<BoundaryNode *>(rootNode_);
          if (!boundary)
          {
            return;
          }
          ComponentContext rootContext;
          boundary->compose(rootContext, event);
        }

        void teardownComposition()
        {
          if (!composed_)
          {
            return;
          }
          if (platformController_)
          {
            platformController_->destroy();
          }
          composed_ = false;
          if (rootNode_)
          {
            delete rootNode_;
            rootNode_ = 0;
          }
        }

        // Default constructor intentionally not implemented to forbid rootless scenes
        Scene();
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_SCENE_HPP
