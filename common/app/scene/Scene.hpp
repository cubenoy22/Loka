#ifndef LOKA_CORE2_SCENE_SCENE_HPP
#define LOKA_CORE2_SCENE_SCENE_HPP

#include "loka/core/State.hpp"
#include <cassert>
#if defined(LOKA_DEBUG_RECOMPOSE) && !defined(LOKA_RETRO68)
#include "loka/platform/DebugLog.hpp"
#endif
#include "app/scene/Node.hpp" // Required for NodeDefinitionBase.
#include "app/scene/PlatformController.hpp"
#include "app/scene/ComponentContext.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/PlatformApplyPlan.hpp"
#include "app/scene/SceneDirector.hpp"
#include "app/scene/node/Boundary.hpp"
#include "loka/core/Profiler.hpp"
#include "loka/dsl/NextTickTracker.hpp"
#include "loka/dsl/CompositionDiff.hpp"

class Window;

enum SceneLifecycle
{
  ON_CREATE = 0,
  ON_ATTACH = 1,
  ON_DETACH = 2,
  ON_DESTROY = 3
};

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class SceneTestAccess;
    }
  }

  namespace app
  {
    namespace scene
    {
      // Forward declaration only. Include the concrete type where needed.
      struct NodeComposition;

      class Scene
      {
      public:
        struct SceneCompositionDiff : public loka::dsl::CompositionDiff
        {
          SceneCompositionDiff() : loka::dsl::CompositionDiff(), flags(NODE_DIRTY_NONE)
          {
            fullRebuild = false;
          }
          void clear()
          {
            loka::dsl::CompositionDiff::clear();
            flags = NODE_DIRTY_NONE;
            fullRebuild = false;
          }
          NodeDirtyFlags flags;
        };
        // Accept Boundary definitions only (compile-time check via IsBoundaryDefinition).
        template <class DefT>
        explicit Scene(DefT *def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), lastApplyPlan_()
        {
          assert(def && "Scene requires a root definition");
          director_.attach(this);
        }
        // Construct from NodeDefinitionBase and auto-wrap non-boundary roots.
        explicit Scene(NodeDefinitionBase *def)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), lastApplyPlan_()
        {
          assert(def && "Scene requires a root definition");
          director_.attach(this);
        }
        // Clone and take ownership of the root definition.
        template <class DefT>
        explicit Scene(const DefT &def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def.clone()), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), lastApplyPlan_()
        {
          director_.attach(this);
        }
        virtual ~Scene()
        {
          director_.detach();
          updateLifecycle(ON_DESTROY);
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
        SceneDirector &director() { return director_; }
        const SceneDirector &director() const { return director_; }
        size_t liveNodeCount() const
        {
          return countLiveNodes(rootNode_);
        }

        // Expose lifecycle and attached states for observation.
        loka::core::State<SceneLifecycle> *getLifecycleState() { return &lifecycle_; }
        loka::core::State<bool> *getAttachedState() { return &attached_; }

        // Public wrappers for controlled lifecycle and attached updates.
        // SceneManager is the main caller; other callers must manage side effects via StateTracker.
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

        void requestInvalidate(NodeDirtyFlags flags = NODE_DIRTY_PROPS)
        {
#if defined(LOKA_DEBUG_RECOMPOSE) && !defined(LOKA_RETRO68)
          const bool alreadyPending = nextTickTracker_.hasPendingRequest();
          if (alreadyPending)
          {
            loka::platform::DebugLogRecomposeMerged(static_cast<void *>(this));
          }
          else
          {
            loka::platform::DebugLogRecomposeQueued(static_cast<void *>(this));
          }
#endif
          if (flags == NODE_DIRTY_NONE)
          {
            flags = NODE_DIRTY_PROPS;
          }
          compositionDiff_.flags = static_cast<NodeDirtyFlags>(compositionDiff_.flags | flags);
          if ((flags & (NODE_DIRTY_CHILD | NODE_DIRTY_INITIAL)) != 0)
          {
            compositionDiff_.fullRebuild = true;
          }
          nextTickTracker_.request();
        }

        void requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately)
        {
          director_.requestBoundaryUpdate(boundary, flags, flushImmediately);
        }

        bool flushInvalidation()
        {
          return nextTickTracker_.run(&Scene::RefreshThunk, &Scene::ApplyThunk, this);
        }

        bool hasPendingInvalidation() const
        {
          return nextTickTracker_.hasPendingRequest();
        }

        void invalidate(NodeDirtyFlags flags = NODE_DIRTY_PROPS)
        {
          requestInvalidate(flags);
          flushInvalidation();
        }

      protected:
        void setAttached(bool v) { attached_.set(v); }
        void setLifecycle(SceneLifecycle v) { lifecycle_.set(v); }

        loka::core::MutableState<SceneLifecycle> lifecycle_;
        loka::core::MutableState<bool> attached_;
        NodeDefinitionBase *rootDefinition_;
        Node *rootNode_;
        IPlatformController *platformController_;
        Window *window_;
        bool mounted_;
        bool composed_;
        loka::dsl::NextTickTracker nextTickTracker_;
        SceneCompositionDiff compositionDiff_;
        SceneDirector director_;
        PlatformApplyPlan lastApplyPlan_;

        // SceneManager owns lifecycle_/attached mutations.
        friend class SceneManager;
        friend class SceneDirector;
        friend class ::loka::dsl::testing::SceneTestAccess;

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
            this->clearChildren();
            if (!def_)
            {
              return;
            }
            NodeComposition &composition = this->beginComposition(context);
            composition.declare(*def_);
            Node *child = composition.createNodeTree();
            if (child)
            {
              this->addChild(child);
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
          rootContext.setPlatformController(platformController_);
          rootContext.setScene(this);
          rootContext.setWindow(this->getWindow());
          boundary->compose(rootContext, event);
          platformController_->onChange(rootNode_, NODE_DIRTY_INITIAL, true);
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
          rootContext.setPlatformController(platformController_);
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
          NodeDirtyFlags boundaryFlags = director_.aggregatePendingBoundaryFlags();
          if (boundaryFlags != NODE_DIRTY_NONE)
          {
            compositionDiff_.flags = static_cast<NodeDirtyFlags>(compositionDiff_.flags | boundaryFlags);
          }
          compositionDiff_.valid = true;
          compositionDiff_.fullRebuild = (compositionDiff_.flags & (NODE_DIRTY_CHILD | NODE_DIRTY_INITIAL)) != 0;
          if (compositionDiff_.flags == NODE_DIRTY_NONE)
          {
            compositionDiff_.flags = NODE_DIRTY_PROPS;
            compositionDiff_.fullRebuild = false;
          }
          notifyComposeEvent(COMPOSE_EVENT_UPDATE);
          if (pendingUpdateRootsRequireLayout(director_))
          {
            compositionDiff_.flags = static_cast<NodeDirtyFlags>(compositionDiff_.flags | NODE_DIRTY_LAYOUT);
          }
          if (compositionDiff_.fullRebuild && pendingUpdateRootsCanApplyLocalCompositionDiff(director_))
          {
            compositionDiff_.fullRebuild = false;
          }
          else if (compositionDiff_.fullRebuild && rootBoundaryCanApplyLocalCompositionDiff(rootNode_))
          {
            compositionDiff_.fullRebuild = false;
          }
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
          lastApplyPlan_ = buildPlatformApplyPlan(rootNode_, director_, compositionDiff_);
          applyPendingBoundaryUpdates(director_);
          platformController_->onChange(rootNode_, flags, compositionDiff_.fullRebuild);
          compositionDiff_.clear();
          director_.clearPendingBoundaryRequest();
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

        static bool rootBoundaryCanApplyLocalCompositionDiff(Node *node)
        {
          if (!node)
          {
            return false;
          }
          BoundaryNode *boundary = node->asBoundary();
          return boundary && boundary->canApplyLocalCompositionDiff();
        }

        static bool pendingUpdateRootsCanApplyLocalCompositionDiff(const SceneDirector &director)
        {
          bool sawRoot = false;
          BoundaryNode *root = director.firstPendingUpdateRoot();
          while (root)
          {
            const BoundaryComposeResult &result = root->composeResult();
            if (!result.composed || !result.preservedNativeContexts)
            {
              return false;
            }
            sawRoot = true;
            root = director.nextPendingUpdateRoot(root);
          }
          return sawRoot;
        }

        static bool pendingUpdateRootsRequireLayout(const SceneDirector &director)
        {
          BoundaryNode *root = director.firstPendingUpdateRoot();
          while (root)
          {
            const BoundaryUpdateResult &result = root->updateResult();
            if (result.actualBoundsChanged || result.affectsAncestorLayout)
            {
              return true;
            }
            root = director.nextPendingUpdateRoot(root);
          }
          return false;
        }

        static PlatformApplyPlan buildPlatformApplyPlan(Node *rootNode,
                                                        const SceneDirector &director,
                                                        const SceneCompositionDiff &diff)
        {
          PlatformApplyPlan plan;
          plan.structureChanged = (diff.flags & (NODE_DIRTY_CHILD | NODE_DIRTY_INITIAL)) != 0;
          plan.layoutChanged = (diff.flags & NODE_DIRTY_LAYOUT) != 0 || pendingUpdateRootsRequireLayout(director);
          if (diff.flags != NODE_DIRTY_NONE)
          {
            plan.paintKind = PlatformApplyPlan::PAINT_LOCAL;
          }
          BoundaryNode *rootBoundary = rootNode ? rootNode->asBoundary() : 0;
          BoundaryNode *firstPendingRoot = director.firstPendingUpdateRoot();
          plan.layoutRoot = firstPendingRoot ? firstPendingRoot : rootBoundary;
          plan.paintRoot = firstPendingRoot ? firstPendingRoot : rootBoundary;
          return plan;
        }

        static void applyPendingBoundaryUpdates(const SceneDirector &director)
        {
          BoundaryNode *root = director.firstPendingUpdateRoot();
          while (root)
          {
            root->beginPlatformApply();
            root->applyPendingUpdate();
            root->endPlatformApply();
            root = director.nextPendingUpdateRoot(root);
          }
        }

        static size_t countLiveNodes(Node *node)
        {
          if (!node)
          {
            return 0;
          }
          size_t count = 1;
          INestable *nestable = node->asNestable();
          if (!nestable)
          {
            return count;
          }
          loka::dsl::CompositionCursor<Node> it(nestable->childrenHead(), nestable->childrenCount());
          for (Node *child = it.next(); child; child = it.next())
          {
            count += countLiveNodes(child);
          }
          return count;
        }

        // Default constructor intentionally not implemented to forbid rootless scenes
        Scene();
      };

      inline SceneDirector::SceneDirector()
          : scene_(0), lastRequestedBoundary_(0), pendingBoundaryFlags_(NODE_DIRTY_NONE), pendingBoundariesHead_(0), pendingBoundariesTail_(0)
      {
      }

      inline void SceneDirector::attach(Scene *scene)
      {
        scene_ = scene;
        clearPendingBoundaryRequest();
      }

      inline void SceneDirector::detach()
      {
        scene_ = 0;
        clearPendingBoundaryRequest();
      }

      inline void SceneDirector::requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately)
      {
        if (!scene_)
        {
          return;
        }
        if (flags == NODE_DIRTY_NONE)
        {
          flags = NODE_DIRTY_PROPS;
        }
        lastRequestedBoundary_ = boundary;
        pendingBoundaryFlags_ = static_cast<NodeDirtyFlags>(pendingBoundaryFlags_ | flags);
        enqueueBoundary(boundary);
        if (flushImmediately)
        {
          scene_->invalidate(flags);
          return;
        }
        scene_->requestInvalidate(flags);
      }

      inline BoundaryNode *SceneDirector::lastRequestedBoundary() const
      {
        return lastRequestedBoundary_;
      }

      inline NodeDirtyFlags SceneDirector::pendingBoundaryFlags() const
      {
        return pendingBoundaryFlags_;
      }

      inline BoundaryNode *SceneDirector::pendingBoundariesHead() const
      {
        return pendingBoundariesHead_;
      }

      inline NodeDirtyFlags SceneDirector::aggregatePendingBoundaryFlags() const
      {
        NodeDirtyFlags flags = NODE_DIRTY_NONE;
        BoundaryNode *boundary = pendingBoundariesHead_;
        while (boundary)
        {
          flags = static_cast<NodeDirtyFlags>(flags | boundary->pendingDirtyFlags());
          boundary = boundary->nextPendingBoundary();
        }
        return flags;
      }

      inline BoundaryNode *SceneDirector::topMostRequestedBoundary(BoundaryNode *boundary) const
      {
        if (!boundary)
        {
          return 0;
        }
        BoundaryNode *top = boundary;
        BoundaryNode *parent = boundary->parentBoundary();
        while (parent)
        {
          if (!parent->isUpdateRequested())
          {
            break;
          }
          top = parent;
          parent = parent->parentBoundary();
        }
        return top;
      }

      inline bool SceneDirector::isBoundaryUpdateRoot(BoundaryNode *boundary) const
      {
        if (!boundary || !boundary->isUpdateRequested())
        {
          return false;
        }
        return topMostRequestedBoundary(boundary) == boundary;
      }

      inline BoundaryNode *SceneDirector::firstPendingUpdateRoot() const
      {
        return nextPendingUpdateRoot(0);
      }

      inline BoundaryNode *SceneDirector::nextPendingUpdateRoot(BoundaryNode *afterRoot) const
      {
        bool startSearching = (afterRoot == 0);
        BoundaryNode *boundary = pendingBoundariesHead_;
        while (boundary)
        {
          BoundaryNode *root = topMostRequestedBoundary(boundary);
          if (root)
          {
            if (!startSearching)
            {
              if (root == afterRoot)
              {
                startSearching = true;
              }
              boundary = boundary->nextPendingBoundary();
              continue;
            }

            bool seenEarlier = false;
            BoundaryNode *previous = pendingBoundariesHead_;
            while (previous && previous != boundary)
            {
              if (topMostRequestedBoundary(previous) == root)
              {
                seenEarlier = true;
                break;
              }
              previous = previous->nextPendingBoundary();
            }
            if (!seenEarlier)
            {
              return root;
            }
          }
          boundary = boundary->nextPendingBoundary();
        }
        return 0;
      }

      inline void SceneDirector::clearPendingBoundaryRequest()
      {
        BoundaryNode *boundary = pendingBoundariesHead_;
        while (boundary)
        {
          BoundaryNode *next = boundary->nextPendingBoundary();
          boundary->clearPendingUpdateState();
          boundary = next;
        }
        lastRequestedBoundary_ = 0;
        pendingBoundaryFlags_ = NODE_DIRTY_NONE;
        pendingBoundariesHead_ = 0;
        pendingBoundariesTail_ = 0;
      }

      inline void SceneDirector::enqueueBoundary(BoundaryNode *boundary)
      {
        if (!boundary || boundary->isUpdateRequested())
        {
          return;
        }
        boundary->setUpdateRequested(true);
        boundary->setNextPendingBoundary(0);
        if (!pendingBoundariesHead_)
        {
          pendingBoundariesHead_ = boundary;
          pendingBoundariesTail_ = boundary;
          return;
        }
        pendingBoundariesTail_->setNextPendingBoundary(boundary);
        pendingBoundariesTail_ = boundary;
      }

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENE_HPP
