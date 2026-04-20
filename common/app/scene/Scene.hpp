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

        struct SceneUpdateCycleState
        {
          SceneUpdateCycleState()
              : compositionDiff(), lastApplyPlan(), pendingSnapshot(), lastAppliedSnapshot()
          {
          }

          void clearPending()
          {
            compositionDiff.clear();
            pendingSnapshot.clear();
          }

          void discardPending()
          {
            clearPending();
          }

          void clearRetainedSnapshots()
          {
            pendingSnapshot.clear();
            lastAppliedSnapshot.clear();
          }

          void beginRefresh(NodeDirtyFlags flags, bool fullRebuild)
          {
            compositionDiff.valid = true;
            compositionDiff.flags = flags;
            compositionDiff.fullRebuild = fullRebuild;
          }

          void recordPendingSnapshot(const SceneDirector::SceneUpdateSnapshot &snapshot)
          {
            pendingSnapshot = snapshot;
            compositionDiff.flags = pendingSnapshot.request.effectiveDirtyFlags;
            compositionDiff.fullRebuild = pendingSnapshot.request.effectiveFullRebuild;
          }

          void completeApply(const PlatformApplyPlan &plan)
          {
            lastApplyPlan = plan;
            lastAppliedSnapshot = pendingSnapshot;
            clearPending();
          }

          NodeDirtyFlags effectiveApplyDirtyFlags() const
          {
            return compositionDiff.flags == NODE_DIRTY_NONE ? NODE_DIRTY_PROPS : compositionDiff.flags;
          }

          SceneCompositionDiff compositionDiff;
          PlatformApplyPlan lastApplyPlan;
          SceneDirector::SceneUpdateSnapshot pendingSnapshot;
          SceneDirector::SceneUpdateSnapshot lastAppliedSnapshot;
        };
        // Accept Boundary definitions only (compile-time check via IsBoundaryDefinition).
        template <class DefT>
        explicit Scene(DefT *def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), updateCycleState_()
        {
          assert(def && "Scene requires a root definition");
          director_.attach(this);
        }
        // Construct from NodeDefinitionBase and auto-wrap non-boundary roots.
        explicit Scene(NodeDefinitionBase *def)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), updateCycleState_()
        {
          assert(def && "Scene requires a root definition");
          director_.attach(this);
        }
        // Clone and take ownership of the root definition.
        template <class DefT>
        explicit Scene(const DefT &def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE), attached_(false), rootDefinition_(def.clone()), rootNode_(0), platformController_(0), window_(0), mounted_(false), composed_(false), director_(), updateCycleState_()
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
        const SceneCompositionDiff &compositionDiff() const { return updateCycleState_.compositionDiff; }
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
          clearMountedUpdateState();
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
            request.boundary = rootBoundary;
            director_.registerBoundaryUpdate(request);
          }
          queueInvalidate();
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
        SceneDirector director_;
        SceneUpdateCycleState updateCycleState_;

        // SceneManager owns lifecycle_/attached mutations.
        friend class SceneManager;
        friend class SceneDirector;
        friend class ::loka::dsl::testing::SceneTestAccess;

      private:
        void queueInvalidate()
        {
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

        void beginPendingRefreshCycle()
        {
          updateCycleState_.beginRefresh(director_.effectiveRequestedDirtyFlags(),
                                         director_.requestedFullRebuild());
        }

        SceneDirector::SceneUpdateSnapshot buildPendingRefreshSnapshot() const
        {
          return director_.buildUpdateSnapshot(rootNode_, this);
        }

        void capturePendingRefreshSnapshot()
        {
          updateCycleState_.recordPendingSnapshot(buildPendingRefreshSnapshot());
        }

        void composePendingRefreshCycle()
        {
          notifyComposeEvent(COMPOSE_EVENT_UPDATE);
        }

        void *sceneIdentity() const
        {
          return static_cast<void *>(const_cast<Scene *>(this));
        }

        void logRefreshDecision() const
        {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          const SceneDirector::SceneUpdateSnapshot &snapshot = updateCycleState_.pendingSnapshot;
          loka::platform::DebugLogSceneDecision(sceneIdentity(),
                                                snapshot.apply.structureRequired() ? 1 : 0,
                                                snapshot.apply.layoutRequired() ? 1 : 0,
                                                snapshot.apply.localCompositionDiffApplicable() ? 1 : 0);
#endif
        }

        void logRefreshFlags() const
        {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneFlags(sceneIdentity(),
                                            "refresh",
                                            static_cast<unsigned int>(updateCycleState_.compositionDiff.flags),
                                            static_cast<unsigned int>(updateCycleState_.pendingSnapshot.request.effectiveDirtyFlags),
                                            updateCycleState_.compositionDiff.fullRebuild ? 1 : 0);
#endif
        }

        void logApplyFlags(NodeDirtyFlags flags) const
        {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneFlags(sceneIdentity(),
                                            "apply",
                                            static_cast<unsigned int>(flags),
                                            static_cast<unsigned int>(director_.aggregateDirtyFlags()),
                                            updateCycleState_.compositionDiff.fullRebuild ? 1 : 0);
#endif
        }

        void executePendingApplyCycle(NodeDirtyFlags flags)
        {
          updateCycleState_.completeApply(buildExecutedApplyPlan(flags));
        }

        PlatformApplyPlan buildExecutedApplyPlan(NodeDirtyFlags flags)
        {
          return director_.executeApplyPlan(rootNode_,
                                            platformController_,
                                            updateCycleState_.pendingSnapshot,
                                            flags,
                                            updateCycleState_.compositionDiff.fullRebuild);
        }

        void clearPendingRefreshCycle()
        {
          updateCycleState_.discardPending();
          director_.clearUpdateTransaction();
        }

        void clearMountedUpdateState()
        {
          updateCycleState_.clearRetainedSnapshots();
          director_.clearUpdateTransaction();
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
          rootContext.setDirtyFlags(updateCycleState_.compositionDiff.flags);
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
            clearPendingRefreshCycle();
            return false;
          }
          beginPendingRefreshCycle();
          composePendingRefreshCycle();
          capturePendingRefreshSnapshot();
          logRefreshDecision();
          logRefreshFlags();
          return true;
        }

        void applyComposition()
        {
          if (!platformController_ || !rootNode_)
          {
            return;
          }
          platformController_->beginApplyCycle();
          const NodeDirtyFlags flags = updateCycleState_.effectiveApplyDirtyFlags();
          logApplyFlags(flags);
          executePendingApplyCycle(flags);
          director_.clearUpdateTransaction();
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
        clearUpdateTransaction();
      }

      inline void SceneDirector::detach()
      {
        scene_ = 0;
        clearUpdateTransaction();
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
        scene_->queueInvalidate();
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

      inline NodeDirtyFlags SceneDirector::requestedDirtyFlags() const
      {
        return updateTransaction_.requestedDirtyFlags();
      }

      inline NodeDirtyFlags SceneDirector::effectiveRequestedDirtyFlags() const
      {
        return updateTransaction_.effectiveRequestedDirtyFlags();
      }

      inline bool SceneDirector::hasRequestedInput() const
      {
        return updateTransaction_.hasRequestedInput();
      }

      inline bool SceneDirector::requestedFullRebuild() const
      {
        return updateTransaction_.requestedFullRebuild();
      }

      inline NodeDirtyFlags SceneDirector::pendingDirtyFlagsForBoundary(const BoundaryNode *boundary) const
      {
        return updateTransaction_.pendingDirtyFlagsForBoundary(boundary);
      }

      inline bool SceneDirector::hasPendingBoundary(const BoundaryNode *boundary) const
      {
        return updateTransaction_.hasPendingBoundary(boundary);
      }

      inline BoundaryNode *SceneDirector::firstPendingBoundary() const
      {
        return updateTransaction_.firstPendingBoundary();
      }

      inline BoundaryNode *SceneDirector::nextPendingBoundary(const BoundaryNode *boundary) const
      {
        return updateTransaction_.nextPendingBoundary(boundary);
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
          const NodeDirtyFlags parentFlags = pendingDirtyFlagsForBoundary(parent);
          const NodeDirtyFlags boundaryFlags = pendingDirtyFlagsForBoundary(boundary);
          if (!hasPendingBoundary(parent) || parentFlags == NODE_DIRTY_NONE)
          {
            break;
          }
          if (parentFlags == NODE_DIRTY_CHILD &&
              boundaryFlags != NODE_DIRTY_NONE)
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
        if (!boundary || !hasPendingBoundary(boundary))
        {
          return false;
        }
        return topMostRequestedBoundary(boundary) == boundary;
      }

      inline BoundaryNode *SceneDirector::firstPendingUpdateRoot() const
      {
        return nextPendingUpdateRoot(0);
      }

      inline SceneDirector::SceneUpdateSnapshot SceneDirector::buildUpdateSnapshot(Node *rootNode,
                                                                                   const Scene *scene) const
      {
        SceneUpdateSnapshot snapshot;
        snapshot.setGeneration(updateTransaction_.snapshotGeneration());
        snapshot.setRequest(updateTransaction_.buildRequestSnapshot(rootNode ? rootNode->asBoundary() : 0,
                                                                    firstPendingUpdateRoot()));
        if (!snapshot.hasGeneration())
        {
          return snapshot;
        }
        snapshot.setApply(buildApplySnapshot(scene));
        snapshot.finalizeAfterApplyAnalysis();
        if (CanRelaxFullRebuildForLocalDiff(snapshot) ||
            CanRelaxFullRebuildForChildOnlyUpdate(snapshot) ||
            CanRelaxFullRebuildForRootBoundary(snapshot))
        {
          snapshot.relaxEffectiveFullRebuild();
        }
        return snapshot;
      }

      inline SceneDirector::SceneUpdateApplySnapshot SceneDirector::buildApplySnapshot(const Scene *scene) const
      {
        SceneUpdateApplySnapshot applySnapshot;
        bool structureRequired = false;
        bool layoutRequired = false;
        bool requiresCompositedPaint = false;
        bool hasOpaqueLocalPaint = true;
        bool canApplyLocalCompositionDiff = true;
        bool sawPaintWork = false;
        bool sawRoot = false;
        BoundaryNode *root = firstPendingUpdateRoot();
        while (root)
        {
          sawRoot = true;
          const BoundaryUpdateResult &updateResult = root->updateResult();
          if (updateResult.actualBoundsChanged || updateResult.affectsAncestorLayout)
          {
            layoutRequired = true;
          }
          if (updateResult.requiresCompositedPaint())
          {
            requiresCompositedPaint = true;
            hasOpaqueLocalPaint = false;
          }
          else if (updateResult.hasPaintWork())
          {
            sawPaintWork = true;
            if (!updateResult.hasOpaqueCoverageHint() || !updateResult.opaqueCoverageHintValue())
            {
              hasOpaqueLocalPaint = false;
            }
          }

          const BoundaryComposeResult &composeResult = root->composeResult();
          const NodeCompositionDiff *diff = root->localCompositionDiff();
          const bool diffEmpty = diff && diff->empty();
          const bool diffCompatibleRetainOnly = diff && diff->isCompatibleRetainOnly();
          const NodeDirtyFlags effectiveDirtyFlags =
              static_cast<NodeDirtyFlags>(pendingDirtyFlagsForBoundary(root) | composeResult.dirtyFlagsSeen);
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
                                                       static_cast<unsigned int>(composeResult.dirtyFlagsSeen),
                                                       composeResult.composed ? 1 : 0,
                                                       composeResult.preservedNativeContexts ? 1 : 0);
          loka::platform::DebugLogSceneRootDiffShape(static_cast<void *>(root->scene()),
                                                    static_cast<void *>(root),
                                                    diff ? static_cast<int>(diff->entryCount()) : 0,
                                                    (diff && diff->hasIncompatibleRetain()) ? 1 : 0,
                                                    (diff && diff->isCompatibleRetainOnly()) ? 1 : 0,
                                                    (diff && diff->isStableRetainOnly()) ? 1 : 0);
#endif
          if (!composeResult.composed)
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
            if ((effectiveDirtyFlags & NODE_DIRTY_CHILD) == 0)
            {
              structureRequired = true;
            }
          }
          else if (!diff)
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
            if ((effectiveDirtyFlags & NODE_DIRTY_CHILD) == 0)
            {
              structureRequired = true;
            }
          }
          else if ((effectiveDirtyFlags & NODE_DIRTY_CHILD) != 0 && diffEmpty)
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
          }
          else
          {
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
              structureRequired = true;
            }
          }

          if (!composeResult.composed || !composeResult.preservedNativeContexts)
          {
            canApplyLocalCompositionDiff = false;
          }

          root = nextPendingUpdateRoot(root);
        }

        if (requiresCompositedPaint)
        {
          hasOpaqueLocalPaint = false;
        }
        else
        {
          hasOpaqueLocalPaint = sawPaintWork && hasOpaqueLocalPaint;
        }
        canApplyLocalCompositionDiff = canApplyLocalCompositionDiff && sawRoot;
        applySnapshot.setRequirements(layoutRequired,
                                      structureRequired,
                                      requiresCompositedPaint,
                                      hasOpaqueLocalPaint,
                                      canApplyLocalCompositionDiff);
        return applySnapshot;
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

      inline PlatformApplyPlan SceneDirector::executeApplyPlan(Node *rootNode,
                                                               IPlatformController *platformController,
                                                               const SceneUpdateSnapshot &snapshot,
                                                               NodeDirtyFlags globalDirtyFlags,
                                                               bool fullRebuild) const
      {
        PlatformApplyPlan plan = buildPlatformApplyPlan(snapshot);
        applyPendingBoundaryUpdates(rootNode, plan);
        if (!shouldSkipGlobalChange(platformController, plan) && platformController)
        {
          platformController->onChange(rootNode, globalDirtyFlags, fullRebuild);
        }
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

      inline void SceneDirector::SceneUpdateTransaction::clearTransaction()
      {
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

      inline SceneDirector::PendingUpdateRootAnalysis::PendingUpdateRootAnalysis(const SceneDirector *director,
                                                                                 BoundaryNode *afterRoot)
          : director(director), afterRoot(afterRoot), started(afterRoot == 0)
      {
      }

      inline bool SceneDirector::PendingUpdateRootAnalysis::shouldStart(BoundaryNode *root)
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

      inline bool SceneDirector::PendingUpdateRootAnalysis::hasEquivalentDescendant(BoundaryNode *root) const
      {
        if (!director || !root)
        {
          return false;
        }
        BoundaryNode *boundary = director->firstPendingBoundary();
        while (boundary)
        {
          if (boundary != root && director->hasPendingBoundary(boundary) && IsBoundaryDescendantOf(boundary, root) &&
              director->pendingDirtyFlagsForBoundary(boundary) == director->pendingDirtyFlagsForBoundary(root))
          {
            const BoundaryComposeResult &candidateResult = boundary->composeResult();
            if (candidateResult.composed && candidateResult.preservedNativeContexts)
            {
              return true;
            }
          }
          boundary = director->nextPendingBoundary(boundary);
        }
        return false;
      }

      inline bool SceneDirector::PendingUpdateRootAnalysis::hasSeenRootBefore(BoundaryNode *boundary, BoundaryNode *root) const
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
          previous = director->nextPendingBoundary(previous);
        }
        return false;
      }

      inline bool SceneDirector::PendingUpdateRootAnalysis::shouldSkip(BoundaryNode *boundary, BoundaryNode *root) const
      {
        if (!director || !boundary || !root)
        {
          return true;
        }
        if (hasSeenRootBefore(boundary, root))
        {
          return true;
        }
        if ((director->pendingDirtyFlagsForBoundary(root) & NODE_DIRTY_CHILD) != 0 &&
            hasEquivalentDescendant(root))
        {
          return true;
        }
        return false;
      }

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
        PendingUpdateRootAnalysis analysis(this, afterRoot);
        BoundaryNode *boundary = updateTransaction_.firstPendingBoundary();
        while (boundary)
        {
          BoundaryNode *root = topMostRequestedBoundary(boundary);
          if (root)
          {
            if (!analysis.shouldStart(root))
            {
              boundary = updateTransaction_.nextPendingBoundary(boundary);
              continue;
            }

            if (!analysis.shouldSkip(boundary, root))
            {
              return root;
            }
          }
          boundary = updateTransaction_.nextPendingBoundary(boundary);
        }
        return 0;
      }

      inline void SceneDirector::clearUpdateTransaction()
      {
        updateTransaction_.clearTransaction();
      }

      inline void SceneDirector::SceneUpdateTransaction::enqueueBoundaryUpdate(const BoundaryUpdateRequest &request)
      {
        BoundaryNode *boundary = request.boundary;
        if (!boundary)
        {
          return;
        }
        enqueueSceneRequest(request.flags);
        enqueueProjectionTarget(boundary, request.flags);
      }

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENE_HPP
