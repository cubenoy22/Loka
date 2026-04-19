#ifndef LOKA_CORE2_SCENE_SCENE_HPP
#define LOKA_CORE2_SCENE_SCENE_HPP

#include "loka/core/State.hpp"
#include <cassert>
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
#include "loka/platform/DebugLog.hpp"
#endif
#include "app/scene/Node.hpp" // Required for NodeDefinitionBase.
#include "app/scene/PlatformController.hpp"
#include "app/scene/ComponentContext.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/PlatformApplyPlan.hpp"
#include "app/scene/SceneDirector.hpp"
#include "app/scene/nodes/boundary/Boundary.hpp"
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
      inline static bool CanRelaxFullRebuildForLocalDiff(const SceneDirector::SceneUpdateSnapshot &snapshot);
      inline static bool CanRelaxFullRebuildForChildOnlyUpdate(const SceneDirector::SceneUpdateSnapshot &snapshot);
      inline static bool CanRelaxFullRebuildForRootBoundary(const SceneDirector::SceneUpdateSnapshot &snapshot);
      inline static PlatformApplyPlan::PaintKind ResolvePaintKind(const SceneDirector::SceneUpdateSnapshot &snapshot);

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
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), lastApplyPlan_(), updateSnapshot_(), lastUpdateSnapshot_()
        {
          assert(def && "Scene requires a root definition");
          director_.attach(this);
        }
        // Construct from NodeDefinitionBase and auto-wrap non-boundary roots.
        explicit Scene(NodeDefinitionBase *def)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), lastApplyPlan_(), updateSnapshot_(), lastUpdateSnapshot_()
        {
          assert(def && "Scene requires a root definition");
          director_.attach(this);
        }
        // Clone and take ownership of the root definition.
        template <class DefT>
        explicit Scene(const DefT &def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def.clone()), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), lastApplyPlan_(), updateSnapshot_(), lastUpdateSnapshot_()
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
        const SceneProjectionTransaction &projectionTransaction() const { return director_.projectionTransaction(); }
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
          updateSnapshot_.clear();
          lastUpdateSnapshot_.clear();
          if (rootNode_)
          {
            delete rootNode_;
            rootNode_ = 0;
          }
        }

        void requestInvalidate(NodeDirtyFlags flags = NODE_DIRTY_PROPS)
        {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          const bool alreadyPending = nextTickTracker_.hasPendingRequest();
          if (alreadyPending)
          {
            loka::platform::DebugLogSceneUpdateMerged(static_cast<void *>(this));
          }
          else
          {
            loka::platform::DebugLogSceneUpdateQueued(static_cast<void *>(this));
          }
#endif
          SceneDirector::BoundaryUpdateRequest request;
          request.flags = flags == NODE_DIRTY_NONE ? NODE_DIRTY_PROPS : flags;
          request.flushImmediately = false;
          BoundaryNode *rootBoundary = rootNode_ ? rootNode_->asBoundary() : 0;
          if (rootBoundary)
          {
            rootBoundary->addPendingDirtyFlags(request.flags);
            request.boundary = rootBoundary;
            director_.registerBoundaryUpdate(request);
          }
          queueInvalidate(request.flags);
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
        SceneDirector::SceneUpdateSnapshot updateSnapshot_;
        SceneDirector::SceneUpdateSnapshot lastUpdateSnapshot_;

        // SceneManager owns lifecycle_/attached mutations.
        friend class SceneManager;
        friend class SceneDirector;
        friend class ::loka::dsl::testing::SceneTestAccess;

      private:
        void queueInvalidate(NodeDirtyFlags flags)
        {
          compositionDiff_.flags = static_cast<NodeDirtyFlags>(compositionDiff_.flags | flags);
          if ((flags & NODE_DIRTY_INITIAL) != 0)
          {
            compositionDiff_.fullRebuild = true;
          }
          nextTickTracker_.request();
        }

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
          explicit RootBoundaryWrapper(NodeDefinitionBase *def) : def_(def), composed_(false) {}
          virtual ~RootBoundaryWrapper() {}

        protected:
          void detachExistingChildren(ComponentContext &context)
          {
            loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
            for (Node *child = it.next(); child; child = it.next())
            {
              this->composeTree(child, context, COMPOSE_EVENT_DETACH, this);
            }
          }

          virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
          {
            if (event == COMPOSE_EVENT_DETACH)
            {
              this->detachExistingChildren(context);
              NodeComposition &composition = this->beginComposition(context);
              this->detachNode(composition);
              this->composed_ = false;
              return;
            }
            if (event != COMPOSE_EVENT_ATTACH && event != COMPOSE_EVENT_UPDATE)
            {
              return;
            }
            if (event == COMPOSE_EVENT_UPDATE && this->isFrozen())
            {
              return;
            }
            if (!def_)
            {
              return;
            }
            if (event == COMPOSE_EVENT_UPDATE && !this->composed_)
            {
              return;
            }
            if (event == COMPOSE_EVENT_UPDATE)
            {
              NodeDirtyFlags flags = context.dirtyFlags();
              if (!(flags & NODE_DIRTY_CHILD))
              {
                loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
                for (Node *child = it.next(); child; child = it.next())
                {
                  this->composeTree(child, context, event, this);
                }
                return;
              }
            }
            NodeComposition &composition = this->beginComposition(context);
            if (event == COMPOSE_EVENT_ATTACH)
            {
              this->clearChildren();
              this->nodeArena()->clear();
              this->attachNode(composition);
            }
            {
              NodeComposition::CompositionScope scope(composition);
              composition.declare(*def_);
            }
            this->captureCurrentCompositionSnapshot();
            this->rebuildCurrentCompositionDiff();
            if (event == COMPOSE_EVENT_UPDATE && this->canApplyLocalCompositionDiff() &&
                this->localCompositionDiff()->isCompatibleRetainOnly())
            {
              if (!this->localCompositionDiff()->isStableRetainOnly())
              {
                if (!this->applyCurrentRootDefinitionPropsToLiveRoot())
                {
                  for (NodeCompositionDiff::Entry *entry = this->localCompositionDiff()->entriesHead(); entry;
                       entry = entry->nextInComposition)
                  {
                    if (!entry->equivalentProps)
                    {
                      this->applyCurrentDefinitionPropsToLiveChild(entry->tag);
                    }
                  }
                }
              }
              this->promoteCurrentCompositionSnapshot();
              loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
              for (Node *child = it.next(); child; child = it.next())
              {
                this->composeTree(child, context, event, this);
              }
              return;
            }
            if (event == COMPOSE_EVENT_UPDATE && this->canApplyLocalCompositionDiff())
            {
              std::vector<Node *> retainedChildren;
              if (this->rebuildCompositionChildrenFromCurrentSnapshot(context, retainedChildren) ||
                  this->rebuildCompositionRootFromCurrentSnapshot(context, retainedChildren))
              {
                this->promoteCurrentCompositionSnapshot();
                for (size_t i = 0; i < retainedChildren.size(); ++i)
                {
                  if (retainedChildren[i])
                  {
                    this->composeTree(retainedChildren[i], context, event, this);
                  }
                }
                return;
              }
            }
            this->promoteCurrentCompositionSnapshot();
            if (event == COMPOSE_EVENT_UPDATE)
            {
              this->detachExistingChildren(context);
              this->clearChildren();
              this->nodeArena()->clear();
            }
            context.setComposition(&composition);
            Node *child = composition.createNodeTree();
            if (child)
            {
              this->addChild(child);
              this->composeTree(child, context, event, this);
            }
            context.setComposition(0);
            this->composed_ = true;
          }

        private:
          NodeDefinitionBase *def_;
          bool composed_;
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
          if (rootDefinition_ && !rootDefinition_->isBoundary())
          {
            BoundaryNode::composeSubtree(rootNode_, rootContext, event, 0);
          }
          else
          {
            prepareRootBoundaryCompose(boundary, rootContext, event);
            boundary->compose(rootContext, event);
            completeRootBoundaryCompose(boundary);
          }
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
          if (rootDefinition_ && !rootDefinition_->isBoundary())
          {
            BoundaryNode::composeSubtree(rootNode_, rootContext, event, 0);
          }
          else
          {
            prepareRootBoundaryCompose(boundary, rootContext, event);
            boundary->compose(rootContext, event);
            completeRootBoundaryCompose(boundary);
          }
        }

        static void prepareRootBoundaryCompose(BoundaryNode *boundary,
                                               ComponentContext &rootContext,
                                               ComposeEvent event)
        {
          if (!boundary)
          {
            return;
          }
          boundary->setParentBoundary(0);
          boundary->clearObservedDirtyFlags();
          if (event != COMPOSE_EVENT_UPDATE)
          {
            boundary->clearPhaseResults();
          }
          boundary->beginObservedStatePass();
          boundary->beginComposeResult(event, rootContext.dirtyFlags());
        }

        static void completeRootBoundaryCompose(BoundaryNode *boundary)
        {
          if (!boundary)
          {
            return;
          }
          boundary->completeComposeResult(boundary->canPreserveNativeContexts());
        }

        bool refreshComposition()
        {
          if (!mounted_ || !platformController_ || !composed_)
          {
            compositionDiff_.clear();
            updateSnapshot_.clear();
            return false;
          }
          compositionDiff_.valid = true;
          compositionDiff_.fullRebuild = (compositionDiff_.flags & (NODE_DIRTY_CHILD | NODE_DIRTY_INITIAL)) != 0;
          if (compositionDiff_.flags == NODE_DIRTY_NONE)
          {
            compositionDiff_.flags = NODE_DIRTY_PROPS;
            compositionDiff_.fullRebuild = false;
          }
          notifyComposeEvent(COMPOSE_EVENT_UPDATE);
          updateSnapshot_ = director_.buildUpdateSnapshot(rootNode_, compositionDiff_.flags, compositionDiff_.fullRebuild, this);
          compositionDiff_.flags = updateSnapshot_.request.effectiveDirtyFlags;
          compositionDiff_.fullRebuild = updateSnapshot_.request.effectiveFullRebuild;
          const bool requiresStructure = updateSnapshot_.apply.structureRequired();
          const bool canApplyLocalDiff = updateSnapshot_.apply.localCompositionDiffApplicable();
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneDecision(static_cast<void *>(this),
                                                requiresStructure ? 1 : 0,
                                                updateSnapshot_.apply.layoutRequired() ? 1 : 0,
                                                canApplyLocalDiff ? 1 : 0);
#endif
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneFlags(static_cast<void *>(this),
                                            "refresh",
                                            static_cast<unsigned int>(compositionDiff_.flags),
                                            static_cast<unsigned int>(updateSnapshot_.request.effectiveDirtyFlags),
                                            compositionDiff_.fullRebuild ? 1 : 0);
#endif
          return true;
        }

        void applyComposition()
        {
          if (!platformController_ || !rootNode_)
          {
            return;
          }
          platformController_->beginApplyCycle();
          NodeDirtyFlags flags = compositionDiff_.flags;
          if (flags == NODE_DIRTY_NONE)
          {
            flags = NODE_DIRTY_PROPS;
          }
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneFlags(static_cast<void *>(this),
                                            "apply",
                                            static_cast<unsigned int>(flags),
                                            static_cast<unsigned int>(director_.aggregateDirtyFlags()),
                                            compositionDiff_.fullRebuild ? 1 : 0);
#endif
          lastApplyPlan_ = director_.buildPlatformApplyPlan(updateSnapshot_);
          lastUpdateSnapshot_ = updateSnapshot_;
          director_.applyPendingBoundaryUpdates(rootNode_, lastApplyPlan_);
          if (!director_.shouldSkipGlobalChange(platformController_, lastApplyPlan_))
          {
            platformController_->onChange(rootNode_, flags, compositionDiff_.fullRebuild);
          }
          compositionDiff_.clear();
          updateSnapshot_.clear();
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
          : scene_(0), updateTransaction_()
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
        const BoundaryUpdateRequest request = normalizeBoundaryUpdateRequest(boundary, flags, flushImmediately);
        registerBoundaryUpdate(request);
        applyBoundaryUpdateRequest(request);
      }

      inline SceneDirector::BoundaryUpdateRequest SceneDirector::normalizeBoundaryUpdateRequest(BoundaryNode *boundary,
                                                                                                NodeDirtyFlags flags,
                                                                                                bool flushImmediately) const
      {
        BoundaryUpdateRequest request(boundary, flags, flushImmediately);
        if (request.flags == NODE_DIRTY_NONE)
        {
          request.flags = NODE_DIRTY_PROPS;
        }
        return request;
      }

      inline void SceneDirector::registerBoundaryUpdate(const BoundaryUpdateRequest &request)
      {
        if (!scene_ || !request.boundary)
        {
          return;
        }
        updateTransaction_.enqueueBoundaryUpdate(request);
      }

      inline void SceneDirector::applyBoundaryUpdateRequest(const BoundaryUpdateRequest &request) const
      {
        if (!scene_)
        {
          return;
        }
        scene_->queueInvalidate(request.flags);
        if (request.flushImmediately)
        {
          scene_->flushInvalidation();
        }
      }

      inline const SceneProjectionTransaction &SceneDirector::projectionTransaction() const
      {
        return updateTransaction_.projectionTransaction();
      }

      inline NodeDirtyFlags SceneDirector::aggregateDirtyFlags() const
      {
        return updateTransaction_.aggregateDirtyFlags();
      }

      inline BoundaryNode *SceneDirector::firstPendingBoundary() const
      {
        return updateTransaction_.firstPendingBoundary();
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
          if (!parent->isUpdateRequested() || parent->pendingDirtyFlags() == NODE_DIRTY_NONE)
          {
            break;
          }
          if (parent->pendingDirtyFlags() == NODE_DIRTY_CHILD &&
              boundary->pendingDirtyFlags() != NODE_DIRTY_NONE)
          {
            break;
          }
          top = parent;
          boundary = parent;
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

      inline bool SceneDirector::requiresLayout() const
      {
        BoundaryNode *root = firstPendingUpdateRoot();
        while (root)
        {
          const BoundaryUpdateResult &result = root->updateResult();
          if (result.actualBoundsChanged || result.affectsAncestorLayout)
          {
            return true;
          }
          root = nextPendingUpdateRoot(root);
        }
        return false;
      }

      inline bool SceneDirector::requiresCompositedPaint() const
      {
        BoundaryNode *root = firstPendingUpdateRoot();
        while (root)
        {
          const BoundaryUpdateResult &result = root->updateResult();
          if (result.requiresCompositedPaint())
          {
            return true;
          }
          root = nextPendingUpdateRoot(root);
        }
        return false;
      }

      inline bool SceneDirector::requiresStructure(const Scene *scene) const
      {
        BoundaryNode *root = firstPendingUpdateRoot();
        while (root)
        {
          const BoundaryComposeResult &result = root->composeResult();
          const NodeCompositionDiff *diff = root->localCompositionDiff();
          const bool diffEmpty = diff && diff->empty();
          const bool diffCompatibleRetainOnly = diff && diff->isCompatibleRetainOnly();
          const NodeDirtyFlags effectiveDirtyFlags =
              static_cast<NodeDirtyFlags>(root->pendingDirtyFlags() | result.dirtyFlagsSeen);
          if (!result.composed)
          {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
            loka::platform::DebugLogSceneStructureRoot(static_cast<void *>(const_cast<Scene *>(scene)),
                                                       static_cast<void *>(root),
                                                       static_cast<unsigned int>(effectiveDirtyFlags),
                                                       0,
                                                       diff ? 1 : 0,
                                                       diffEmpty ? 1 : 0,
                                                       diffCompatibleRetainOnly ? 1 : 0,
                                                       ((effectiveDirtyFlags & NODE_DIRTY_CHILD) != 0) ? 0 : 1);
#endif
            if ((effectiveDirtyFlags & NODE_DIRTY_CHILD) != 0)
            {
              root = nextPendingUpdateRoot(root);
              continue;
            }
            return true;
          }
          if (!diff)
          {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
            loka::platform::DebugLogSceneStructureRoot(static_cast<void *>(const_cast<Scene *>(scene)),
                                                       static_cast<void *>(root),
                                                       static_cast<unsigned int>(effectiveDirtyFlags),
                                                       1,
                                                       0,
                                                       0,
                                                       0,
                                                       ((effectiveDirtyFlags & NODE_DIRTY_CHILD) != 0) ? 0 : 1);
#endif
            if ((effectiveDirtyFlags & NODE_DIRTY_CHILD) != 0)
            {
              root = nextPendingUpdateRoot(root);
              continue;
            }
            return true;
          }
          if ((effectiveDirtyFlags & NODE_DIRTY_CHILD) != 0 && diffEmpty)
          {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
            loka::platform::DebugLogSceneStructureRoot(static_cast<void *>(const_cast<Scene *>(scene)),
                                                       static_cast<void *>(root),
                                                       static_cast<unsigned int>(effectiveDirtyFlags),
                                                       1,
                                                       1,
                                                       1,
                                                       diffCompatibleRetainOnly ? 1 : 0,
                                                       0);
#endif
            root = nextPendingUpdateRoot(root);
            continue;
          }
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneStructureRoot(static_cast<void *>(const_cast<Scene *>(scene)),
                                                     static_cast<void *>(root),
                                                     static_cast<unsigned int>(effectiveDirtyFlags),
                                                     1,
                                                     1,
                                                     diffEmpty ? 1 : 0,
                                                     diffCompatibleRetainOnly ? 1 : 0,
                                                     diffCompatibleRetainOnly ? 0 : 1);
#endif
          if (!diffCompatibleRetainOnly)
          {
            return true;
          }
          root = nextPendingUpdateRoot(root);
        }
        return false;
      }

      inline bool SceneDirector::hasOpaqueLocalPaint() const
      {
        BoundaryNode *root = firstPendingUpdateRoot();
        bool sawPaint = false;
        while (root)
        {
          const BoundaryUpdateResult &result = root->updateResult();
          if (result.requiresCompositedPaint())
          {
            return false;
          }
          if (result.hasPaintWork())
          {
            sawPaint = true;
            if (!result.hasOpaqueCoverageHint() || !result.opaqueCoverageHintValue())
            {
              return false;
            }
          }
          root = nextPendingUpdateRoot(root);
        }
        return sawPaint;
      }

      inline bool SceneDirector::canApplyLocalCompositionDiff() const
      {
        bool sawRoot = false;
        BoundaryNode *root = firstPendingUpdateRoot();
        while (root)
        {
          const BoundaryComposeResult &result = root->composeResult();
          const NodeCompositionDiff *diff = root->localCompositionDiff();
          const INestable *rootNestable = root->asNestable();
          const Node *firstChild = rootNestable ? rootNestable->childrenHead() : 0;
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneRootIdentity(static_cast<void *>(root->scene()),
                                                   static_cast<void *>(root),
                                                   static_cast<unsigned int>(root->kind()),
                                                   root->testId().c_str(),
                                                   root->previousCompositionSnapshot().root() ? 1 : 0,
                                                   root->currentCompositionSnapshot().root() ? 1 : 0,
                                                   root->hasCompositionDiffState() ? 0 : 1,
                                                   rootNestable ? static_cast<unsigned int>(rootNestable->childrenCount()) : 0U,
                                                   firstChild ? static_cast<unsigned int>(firstChild->kind()) : 0U,
                                                   firstChild ? firstChild->testId().c_str() : "");
          loka::platform::DebugLogSceneRootDiffDecision(static_cast<void *>(root->scene()),
                                                       static_cast<void *>(root),
                                                       static_cast<unsigned int>(result.dirtyFlagsSeen),
                                                       result.composed ? 1 : 0,
                                                       result.preservedNativeContexts ? 1 : 0);
          loka::platform::DebugLogSceneRootDiffShape(static_cast<void *>(root->scene()),
                                                    static_cast<void *>(root),
                                                    diff ? static_cast<int>(diff->entryCount()) : 0,
                                                    (diff && diff->hasIncompatibleRetain()) ? 1 : 0,
                                                    (diff && diff->isCompatibleRetainOnly()) ? 1 : 0,
                                                    (diff && diff->isStableRetainOnly()) ? 1 : 0);
#endif
          if (!result.composed || !result.preservedNativeContexts)
          {
            return false;
          }
          sawRoot = true;
          root = nextPendingUpdateRoot(root);
        }
        return sawRoot;
      }

      inline SceneDirector::SceneUpdateSnapshot SceneDirector::buildUpdateSnapshot(Node *rootNode,
                                                                                   NodeDirtyFlags flags,
                                                                                   bool fullRebuild,
                                                                                   const Scene *scene) const
      {
        SceneUpdateSnapshot snapshot;
        snapshot.setGeneration(updateTransaction_.pendingGeneration());
        snapshot.setRequest(updateTransaction_.buildRequestSnapshot(rootNode,
                                                                    firstPendingUpdateRoot(),
                                                                    flags,
                                                                    fullRebuild));
        if (!snapshot.hasGeneration())
        {
          return snapshot;
        }
        snapshot.apply.setRequirements(requiresLayout(),
                                       requiresStructure(scene),
                                       requiresCompositedPaint(),
                                       hasOpaqueLocalPaint(),
                                       canApplyLocalCompositionDiff());
        if (snapshot.apply.layoutRequired())
        {
          snapshot.request.includeDirtyFlags(NODE_DIRTY_LAYOUT);
        }
        if (CanRelaxFullRebuildForLocalDiff(snapshot) ||
            CanRelaxFullRebuildForChildOnlyUpdate(snapshot) ||
            CanRelaxFullRebuildForRootBoundary(snapshot))
        {
          snapshot.request.relaxFullRebuild();
        }
        return snapshot;
      }

      inline PlatformApplyPlan SceneDirector::buildPlatformApplyPlan(const SceneUpdateSnapshot &snapshot) const
      {
        PlatformApplyPlan plan;
        plan.structureChanged = snapshot.requiresStructureChange();
        plan.layoutChanged = snapshot.requiresLayoutChange();
        plan.paintKind = ResolvePaintKind(snapshot);
        plan.setPrimaryRoot(snapshot.request.primaryRoot());
        return plan;
      }

      inline void SceneDirector::applyPendingBoundaryUpdates(Node *rootNode,
                                                             const PlatformApplyPlan &plan) const
      {
        BoundaryNode *root = firstPendingUpdateRoot();
        if (!root)
        {
          root = rootNode ? rootNode->asBoundary() : 0;
        }
        while (root)
        {
          BoundaryApplyPhaseScope applyScope = root->beginApplyPhaseScope();
          PlatformApplyPlan localPlan = plan.forBoundary(root);
          const BoundaryNode::LocalApplyInfo localInfo = root->localApplyInfo(localPlan);
          assert(localPlan.isLocalizedFor(root));
          if (localPlan.hasBoundaryApplyWork(root) && root->scene() && root->scene()->platformController_)
          {
            root->scene()->platformController_->onBoundaryApply(rootNode, root, localInfo, localPlan);
          }
          root->applyPendingUpdate(localPlan);
          root = nextPendingUpdateRoot(root);
        }
      }

      inline bool SceneDirector::shouldSkipGlobalChange(IPlatformController *platformController,
                                                        const PlatformApplyPlan &plan) const
      {
        return platformController &&
               platformController->canSkipGlobalChangeForBoundaryLocalPaint() &&
               plan.isPaintOnlyWork();
      }

      inline void SceneDirector::SceneUpdateTransaction::PendingBoundaryQueue::append(BoundaryNode *boundary)
      {
        if (!boundary)
        {
          return;
        }
        if (!head)
        {
          head = boundary;
          tail = boundary;
          return;
        }
        tail->setNextPendingBoundary(boundary);
        tail = boundary;
      }

      inline void SceneDirector::SceneUpdateTransaction::PendingBoundaryQueue::clear()
      {
        head = 0;
        tail = 0;
      }

      inline void SceneDirector::SceneUpdateTransaction::PendingBoundaryQueue::clearPendingStates()
      {
        BoundaryNode *boundary = first();
        while (boundary)
        {
          BoundaryNode *next = boundary->nextPendingBoundary();
          boundary->clearPendingUpdateState();
          boundary = next;
        }
        clear();
      }

      inline void SceneDirector::SceneUpdateTransaction::enqueuePendingBoundary(BoundaryNode *boundary)
      {
        if (!boundary)
        {
          return;
        }
        boundary->setNextPendingBoundary(0);
        pendingBoundaries.append(boundary);
      }

      inline void SceneDirector::SceneUpdateTransaction::clearPendingState()
      {
        pendingBoundaries.clearPendingStates();
        clear();
      }

      inline static bool IsBoundaryDescendantOf(const BoundaryNode *boundary, const BoundaryNode *ancestor)
      {
        const BoundaryNode *current = boundary ? boundary->parentBoundary() : 0;
        while (current)
        {
          if (current == ancestor)
          {
            return true;
          }
          current = current->parentBoundary();
        }
        return false;
      }

      struct PendingUpdateRootSelector
      {
        PendingUpdateRootSelector(const SceneDirector *director, BoundaryNode *afterRoot)
            : director(director), afterRoot(afterRoot), started(afterRoot == 0)
        {
        }

        bool shouldStart(BoundaryNode *root)
        {
          if (started)
          {
            return true;
          }
          if (root == afterRoot)
          {
            started = true;
            return false;
          }
          return false;
        }

        bool hasEquivalentDescendant(BoundaryNode *root) const
        {
          if (!director || !root)
          {
            return false;
          }
          BoundaryNode *boundary = director->firstPendingBoundary();
          while (boundary)
          {
            if (boundary != root && boundary->isUpdateRequested() && IsBoundaryDescendantOf(boundary, root) &&
                boundary->pendingDirtyFlags() == root->pendingDirtyFlags())
            {
              const BoundaryComposeResult &candidateResult = boundary->composeResult();
              if (candidateResult.composed && candidateResult.preservedNativeContexts)
              {
                return true;
              }
            }
            boundary = boundary->nextPendingBoundary();
          }
          return false;
        }

        bool hasSeenRootBefore(BoundaryNode *boundary, BoundaryNode *root) const
        {
          if (!director || !boundary || !root)
          {
            return false;
          }
          BoundaryNode *previous = director->firstPendingBoundary();
          while (previous && previous != boundary)
          {
            if (director->topMostRequestedBoundary(previous) == root)
            {
              return true;
            }
            previous = previous->nextPendingBoundary();
          }
          return false;
        }

        bool shouldSkip(BoundaryNode *boundary, BoundaryNode *root) const
        {
          if (!director || !boundary || !root)
          {
            return true;
          }
          if (hasSeenRootBefore(boundary, root))
          {
            return true;
          }
          if ((root->pendingDirtyFlags() & NODE_DIRTY_CHILD) != 0 &&
              hasEquivalentDescendant(root))
          {
            return true;
          }
          return false;
        }

        const SceneDirector *director;
        BoundaryNode *afterRoot;
        bool started;
      };

      inline static bool CanRelaxFullRebuildForLocalDiff(const SceneDirector::SceneUpdateSnapshot &snapshot)
      {
        return snapshot.request.effectiveFullRebuild && snapshot.apply.localCompositionDiffApplicable();
      }

      inline static bool CanRelaxFullRebuildForChildOnlyUpdate(const SceneDirector::SceneUpdateSnapshot &snapshot)
      {
        return snapshot.request.effectiveFullRebuild &&
               snapshot.request.hasEffectiveDirtyFlag(NODE_DIRTY_CHILD) &&
               !snapshot.apply.structureRequired();
      }

      inline static bool CanRelaxFullRebuildForRootBoundary(const SceneDirector::SceneUpdateSnapshot &snapshot)
      {
        if (!snapshot.request.effectiveFullRebuild)
        {
          return false;
        }
        BoundaryNode *rootBoundary = snapshot.request.rootBoundary;
        return rootBoundary && rootBoundary->canApplyLocalCompositionDiff();
      }

      inline static PlatformApplyPlan::PaintKind ResolvePaintKind(const SceneDirector::SceneUpdateSnapshot &snapshot)
      {
        if (snapshot.requiresCompositedPaint())
        {
          return PlatformApplyPlan::PAINT_COMPOSITED;
        }
        if (snapshot.requiresOpaqueLocalPaint())
        {
          return PlatformApplyPlan::PAINT_LOCAL_OPAQUE;
        }
        if (snapshot.hasAnyPaintChange())
        {
          return PlatformApplyPlan::PAINT_LOCAL;
        }
        return PlatformApplyPlan::PAINT_NONE;
      }

      inline BoundaryNode *SceneDirector::nextPendingUpdateRoot(BoundaryNode *afterRoot) const
      {
        PendingUpdateRootSelector selector(this, afterRoot);
        BoundaryNode *boundary = updateTransaction_.firstPendingBoundary();
        while (boundary)
        {
          BoundaryNode *root = topMostRequestedBoundary(boundary);
          if (root)
          {
            if (!selector.shouldStart(root))
            {
              boundary = boundary->nextPendingBoundary();
              continue;
            }

            if (!selector.shouldSkip(boundary, root))
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
        updateTransaction_.clearPendingState();
      }

      inline void SceneDirector::SceneUpdateTransaction::enqueueBoundaryUpdate(const BoundaryUpdateRequest &request)
      {
        BoundaryNode *boundary = request.boundary;
        if (!boundary)
        {
          return;
        }
        if (!boundary->isUpdateRequested())
        {
          boundary->setUpdateRequested(true);
          enqueuePendingBoundary(boundary);
        }
        enqueueProjectionTarget(boundary, request.flags);
      }

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENE_HPP
