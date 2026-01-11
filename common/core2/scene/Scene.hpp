#ifndef LOKA_CORE2_SCENE_SCENE_HPP
#define LOKA_CORE2_SCENE_SCENE_HPP

#include "core/State.hpp"
#include <cassert>
#if defined(LOKA_DEBUG_RECOMPOSE) && !defined(LOKA_RETRO68)
#include "loka/platform/DebugLog.hpp"
#endif
#include "core2/scene/Node.hpp" // NodeDefinitionBase 使用のため定義を取得
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/ComponentContext.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "core/Profiler.hpp"
#include "loka/dsl/RefreshLoop.hpp"
#include "loka/dsl/CompositionDiff.hpp"

class Window;

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
        struct SceneCompositionDiff : public loka::dsl::CompositionDiff
        {
          SceneCompositionDiff() : loka::dsl::CompositionDiff(), flags(NODE_DIRTY_NONE) {}
          void clear()
          {
            loka::dsl::CompositionDiff::clear();
            flags = NODE_DIRTY_NONE;
          }
          NodeDirtyFlags flags;
        };
        // Boundary 定義のみを受け付ける (compile-time check via IsBoundaryDefinition)
        template <class DefT>
        explicit Scene(DefT *def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false)
        {
          assert(def && "Scene requires a root definition");
        }
        // NodeDefinitionBase からの構築（非Boundaryは自動ラップ）
        explicit Scene(NodeDefinitionBase *def)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false)
        {
          assert(def && "Scene requires a root definition");
        }
        // ルート定義を clone して所有するオーバーロード
        template <class DefT>
        explicit Scene(const DefT &def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def.clone()), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false)
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
        Window *getWindow() const { return window_; }
        void setWindow(Window *window) { window_ = window; }
        const SceneCompositionDiff &compositionDiff() const { return compositionDiff_; }

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

        void invalidate()
        {
#if defined(LOKA_DEBUG_RECOMPOSE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogRecomposeQueued(static_cast<void *>(this));
#endif
          refreshLoop_.request();
          refreshLoop_.run(&Scene::RefreshThunk, &Scene::ApplyThunk, this);
        }

      protected:
        void setAttached(bool v) { attached_.set(v, true); }
        void setLifecycle(SceneLifecycle v) { lifecycle_.set(v, true); }

        MutableState<SceneLifecycle> lifecycle_;
        MutableState<bool> attached_;
        NodeDefinitionBase *rootDefinition_;
        Node *rootNode_;
        IPlatformController *platformController_;
        Window *window_;
        bool mounted_;
        bool composed_;
        loka::dsl::RefreshLoop refreshLoop_;
        SceneCompositionDiff compositionDiff_;

        // SceneManager2からlifecycle_/attachedを書き換え可能に
        friend class SceneManager2;

      private:
        static bool RefreshThunk(void *userData)
        {
          Scene *scene = static_cast<Scene *>(userData);
          return scene ? scene->refreshComposition() : false;
        }

        static void ApplyThunk(void *userData)
        {
          Scene *scene = static_cast<Scene *>(userData);
          if (scene)
          {
            scene->applyComposition();
          }
        }

        class RootBoundaryWrapper : public BoundaryNode
        {
        public:
          explicit RootBoundaryWrapper(NodeDefinitionBase *def) : def_(def) {}
          virtual ~RootBoundaryWrapper() {}

        protected:
          virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
          {
            if (event != COMPOSE_EVENT_ATTACH)
            {
              return;
            }
            long t0 = declara::core::ProfileTicks();
            this->clearChildren();
            declara::core::gClearChildTicks += declara::core::ProfileTicks() - t0;
            if (!def_)
            {
              return;
            }
            t0 = declara::core::ProfileTicks();
            NodeComposition &composition = this->beginComposition(context);
            declara::core::gBeginCompTicks += declara::core::ProfileTicks() - t0;
            t0 = declara::core::ProfileTicks();
            composition.declare(*def_);
            declara::core::gComposeNodeTicks += declara::core::ProfileTicks() - t0;
            t0 = declara::core::ProfileTicks();
            Node *child = composition.createNodeTree();
            declara::core::gComposeCreateTicks += declara::core::ProfileTicks() - t0;
            if (child)
            {
              t0 = declara::core::ProfileTicks();
              this->addChild(child);
              declara::core::gAddChildTicks += declara::core::ProfileTicks() - t0;
              this->composeTree(child, context, event, this);
            }
          }

        private:
          NodeDefinitionBase *def_;
        };

        void ensureRootNode()
        {
          if (rootNode_)
          {
            return;
          }
          assert(rootDefinition_ && "Scene requires a root definition");
          if (rootDefinition_->isBoundary())
          {
            rootNode_ = rootDefinition_->create();
          }
          else
          {
            rootNode_ = new RootBoundaryWrapper(rootDefinition_);
          }
          assert(rootNode_ && "Scene failed to create root node");
          BoundaryNode *boundary = rootNode_->asBoundary();
          assert(boundary && "Scene root must be a Boundary node");
          boundary->setScene(this);
          boundary->setParentBoundary(0);
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
          BoundaryNode *boundary = rootNode_->asBoundary();
          assert(boundary && "Scene root must be a Boundary node");
          ComponentContext rootContext;
          rootContext.setBoundary(boundary);
          rootContext.setScene(this);
          rootContext.setWindow(this->getWindow());
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
          BoundaryNode *boundary = rootNode_->asBoundary();
          if (!boundary)
          {
            return;
          }
          ComponentContext rootContext;
          rootContext.setBoundary(boundary);
          rootContext.setScene(this);
          rootContext.setWindow(this->getWindow());
          rootContext.setDirtyFlags(compositionDiff_.flags);
          boundary->compose(rootContext, event);
        }

        bool refreshComposition()
        {
          if (!mounted_ || !platformController_ || !composed_)
          {
            compositionDiff_.clear();
            return false;
          }
          compositionDiff_.valid = true;
          compositionDiff_.fullRebuild = false;
          compositionDiff_.flags = NODE_DIRTY_PROPS;
          notifyComposeEvent(COMPOSE_EVENT_UPDATE);
          return true;
        }

        void applyComposition()
        {
          if (!platformController_ || !rootNode_)
          {
            return;
          }
          NodeDirtyFlags flags = compositionDiff_.flags;
          if (flags == NODE_DIRTY_NONE)
          {
            flags = NODE_DIRTY_PROPS;
          }
          platformController_->onChange(rootNode_, flags);
          compositionDiff_.clear();
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

#endif // LOKA_CORE2_SCENE_SCENE_HPP
