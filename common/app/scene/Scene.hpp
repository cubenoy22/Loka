#ifndef LOKA_CORE2_SCENE_SCENE_HPP
#define LOKA_CORE2_SCENE_SCENE_HPP

#include "core/diag/LifecycleAudit.hpp"
#include "core/State.hpp"
#include <cassert>
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
#include "platform/debug/DebugLog.hpp"
#endif
#include "app/scene/Node.hpp" // Required for NodeDefinitionBase.
#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/context/ComponentContext.hpp"
#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/projection/PlatformApplyPlan.hpp"
#include "app/scene/SceneDirector.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "core/Profiler.hpp"
#include "core/scheduler/NextTickTracker.hpp"
#include "core/util/OwnedDef.hpp"
#include "dsl/composition/CompositionDiff.hpp"

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
  } // namespace dsl

  namespace app
  {
    namespace detail
    {
      class SceneRetirePool;
    }

    namespace scene
    {
      // Forward declaration only. Include the concrete type where needed.
      struct NodeComposition;
      inline static bool CanRelaxFullRebuildForLocalDiff(const SceneDirector::SceneUpdateSnapshot &snapshot);
      inline static bool CanRelaxFullRebuildForChildOnlyUpdate(const SceneDirector::SceneUpdateSnapshot &snapshot);
      inline static bool CanRelaxFullRebuildForRootBoundary(const SceneDirector::SceneUpdateSnapshot &snapshot);
      inline static PlatformApplyPlan::PaintKind ResolvePaintKind(const SceneDirector::SceneUpdateSnapshot &snapshot);

      class Scene LOKA_AUDITED(Scene)
      {
      public:
        struct SceneCompositionDiff : public loka::dsl::CompositionDiff
        {
          SceneCompositionDiff()
              : loka::dsl::CompositionDiff(),
                flags(NODE_DIRTY_NONE)
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
          struct RetainedApplyState
          {
            RetainedApplyState()
                : lastApplyPlan(),
                  lastAppliedSnapshot()
            {
            }

            void clear()
            {
              lastApplyPlan.clear();
              lastAppliedSnapshot = SceneDirector::SceneUpdateSnapshot();
            }

            void record(const PlatformApplyPlan &plan, const SceneDirector::SceneUpdateSnapshot &snapshot)
            {
              lastApplyPlan = plan;
              lastAppliedSnapshot = snapshot;
            }

            const PlatformApplyPlan &applyPlan() const
            {
              return lastApplyPlan;
            }

            const SceneDirector::SceneUpdateSnapshot &appliedSnapshot() const
            {
              return lastAppliedSnapshot;
            }

          private:
            PlatformApplyPlan lastApplyPlan;
            SceneDirector::SceneUpdateSnapshot lastAppliedSnapshot;
          };

          SceneUpdateCycleState()
              : compositionDiff(),
                pendingSnapshot(),
                retainedApply()
          {
          }

          void discardPending()
          {
            clearPendingState();
          }

          void discardRetainedState()
          {
            clearPendingState();
            retainedApply.clear();
          }

          void applyRequestSnapshot(const SceneDirector::SceneUpdateRequestSnapshot &request)
          {
            compositionDiff.flags = request.effectiveDirtyFlagsValue();
            compositionDiff.fullRebuild = request.effectiveFullRebuildRequired();
          }

          void beginRefresh(const SceneDirector::SceneUpdateRequestSnapshot &request)
          {
            compositionDiff.valid = true;
            applyRequestSnapshot(request);
          }

          void recordPendingSnapshot(const SceneDirector::SceneUpdateSnapshot &snapshot)
          {
            pendingSnapshot = snapshot;
            applyRequestSnapshot(pendingSnapshot.request());
          }

          void completeApply(const PlatformApplyPlan &plan)
          {
            retainedApply.record(plan, pendingSnapshot);
            clearPendingState();
          }

          NodeDirtyFlags effectiveApplyDirtyFlags() const
          {
            return compositionDiff.flags == NODE_DIRTY_NONE ? NODE_DIRTY_PROPS : compositionDiff.flags;
          }

          NodeDirtyFlags refreshDirtyFlags() const
          {
            return compositionDiff.flags;
          }

          bool refreshFullRebuild() const
          {
            return compositionDiff.fullRebuild;
          }

          NodeDirtyFlags effectivePendingRequestDirtyFlags() const
          {
            return pendingSnapshot.request().effectiveDirtyFlagsValue();
          }

          NodeDirtyFlags aggregateTransactionDirtyFlags() const
          {
            return pendingSnapshot.request().transactionDirtyFlagsValue();
          }

          bool refreshStructureRequired() const
          {
            return pendingSnapshot.apply().structureRequired();
          }

          bool refreshLayoutRequired() const
          {
            return pendingSnapshot.apply().layoutRequired();
          }

          bool refreshLocalCompositionDiffApplicable() const
          {
            return pendingSnapshot.apply().localCompositionDiffApplicable();
          }

          const SceneCompositionDiff &compositionDiffValue() const
          {
            return compositionDiff;
          }

          const PlatformApplyPlan &lastApplyPlanValue() const
          {
            return retainedApply.applyPlan();
          }

          const SceneDirector::SceneUpdateSnapshot &pendingSnapshotValue() const
          {
            return pendingSnapshot;
          }

          const SceneDirector::SceneUpdateSnapshot &lastAppliedSnapshotValue() const
          {
            return retainedApply.appliedSnapshot();
          }

        private:
          void clearPendingState()
          {
            compositionDiff.clear();
            pendingSnapshot = SceneDirector::SceneUpdateSnapshot();
          }

          SceneCompositionDiff compositionDiff;
          SceneDirector::SceneUpdateSnapshot pendingSnapshot;
          RetainedApplyState retainedApply;
        };
        // Accept Boundary definitions only (compile-time check via IsBoundaryDefinition).
        template <class DefT>
        explicit Scene(DefT *def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE),
              attached_(false),
              rootDefinition_(def),
              rootNode_(0),
              platformController_(0),
              window_(0),
              retiredNextScene_(0),
              mounted_(false),
              composed_(false),
              director_(),
              updateCycleState_()
        {
          assert(def && "Scene requires a root definition");
          director_.attach(this);
        }
        // Construct from NodeDefinitionBase and auto-wrap non-boundary roots.
        explicit Scene(NodeDefinitionBase *def)
            : lifecycle_(ON_CREATE),
              attached_(false),
              rootDefinition_(def),
              rootNode_(0),
              platformController_(0),
              window_(0),
              retiredNextScene_(0),
              mounted_(false),
              composed_(false),
              director_(),
              updateCycleState_()
        {
          assert(def && "Scene requires a root definition");
          director_.attach(this);
        }
        // Clone and take ownership of the root definition.
        template <class DefT>
        explicit Scene(const DefT &def, typename DefT::IsBoundaryDefinition * = 0)
            : lifecycle_(ON_CREATE),
              attached_(false),
              rootDefinition_(def.clone()),
              rootNode_(0),
              platformController_(0),
              window_(0),
              retiredNextScene_(0),
              mounted_(false),
              composed_(false),
              director_(),
              updateCycleState_()
        {
          director_.attach(this);
        }
        virtual ~Scene()
        {
          director_.detach();
          updateLifecycle(ON_DESTROY);
          unmount();
          rootDefinition_.reset();
        }

        NodeDefinitionBase *getRootDefinition() const
        {
          return rootDefinition_.get();
        }
        Window *getWindow() const
        {
          return window_;
        }
        void setWindow(Window *window)
        {
          window_ = window;
        }
        const SceneCompositionDiff &compositionDiff() const
        {
          return updateCycleState_.compositionDiffValue();
        }
        SceneDirector &director()
        {
          return director_;
        }
        const SceneDirector &director() const
        {
          return director_;
        }
        size_t liveNodeCount() const
        {
          return countLiveNodes(rootNode_);
        }

        // Expose lifecycle and attached states for observation.
        loka::core::State<SceneLifecycle> *getLifecycleState()
        {
          return &lifecycle_;
        }
        loka::core::State<bool> *getAttachedState()
        {
          return &attached_;
        }

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
        void updateLifecycle(SceneLifecycle v)
        {
          setLifecycle(v);
        }

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
        void setAttached(bool v)
        {
          attached_.set(v);
        }
        void setLifecycle(SceneLifecycle v)
        {
          lifecycle_.set(v);
        }

        loka::core::MutableState<SceneLifecycle> lifecycle_;
        loka::core::MutableState<bool> attached_;
        loka::core::OwnedDef<NodeDefinitionBase> rootDefinition_;
        Node *rootNode_;
        IPlatformController *platformController_;
        Window *window_;
        Scene *retiredNextScene_;
        bool mounted_;
        bool composed_;
        loka::core::NextTickTracker nextTickTracker_;
        SceneDirector director_;
        SceneUpdateCycleState updateCycleState_;

        // SceneManager owns lifecycle_/attached mutations.
        friend class SceneManager;
        friend class loka::app::detail::SceneRetirePool;
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

        bool beginPendingRefreshCycle()
        {
          if (!director_.beginRefreshCycle())
          {
            return false;
          }
          updateCycleState_.beginRefresh(director_.buildRefreshRequestSnapshot(rootNode_));
          return true;
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
          loka::platform::DebugLogSceneDecision(sceneIdentity(),
                                                updateCycleState_.refreshStructureRequired() ? 1 : 0,
                                                updateCycleState_.refreshLayoutRequired() ? 1 : 0,
                                                updateCycleState_.refreshLocalCompositionDiffApplicable() ? 1 : 0);
#endif
        }

        void logRefreshFlags() const
        {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneFlags(
              sceneIdentity(),
              "refresh",
              static_cast<unsigned int>(updateCycleState_.refreshDirtyFlags()),
              static_cast<unsigned int>(updateCycleState_.effectivePendingRequestDirtyFlags()),
              updateCycleState_.refreshFullRebuild() ? 1 : 0);
#endif
        }

        void logApplyFlags(NodeDirtyFlags flags) const
        {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneFlags(
              sceneIdentity(),
              "apply",
              static_cast<unsigned int>(flags),
              static_cast<unsigned int>(updateCycleState_.aggregateTransactionDirtyFlags()),
              updateCycleState_.refreshFullRebuild() ? 1 : 0);
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
                                            updateCycleState_.pendingSnapshotValue(),
                                            flags,
                                            updateCycleState_.refreshFullRebuild());
        }

        void clearPendingRefreshCycle()
        {
          updateCycleState_.discardPending();
          director_.clearUpdateTransaction();
        }

        void clearMountedUpdateState()
        {
          updateCycleState_.discardRetainedState();
          director_.clearUpdateTransaction();
        }

        class RootBoundaryWrapper : public BoundaryNode
        {
        public:
          explicit RootBoundaryWrapper(NodeDefinitionBase *def)
              : def_(def),
                composed_(false)
          {
          }
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
            if (event == COMPOSE_EVENT_UPDATE && this->canApplyLocalCompositionDiff()
                && this->localCompositionDiff()->isCompatibleRetainOnly())
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
              if (this->rebuildCompositionChildrenFromCurrentSnapshot(context, retainedChildren)
                  || this->rebuildCompositionRootFromCurrentSnapshot(context, retainedChildren))
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
          assert(rootDefinition_.isSet() && "Scene requires a root definition");
          if (rootDefinition_->isBoundary())
          {
            rootNode_ = rootDefinition_->create();
          }
          else
          {
            rootNode_ = new RootBoundaryWrapper(rootDefinition_.get());
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
          if (rootDefinition_.isSet() && !rootDefinition_->isBoundary())
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
          rootContext.setDirtyFlags(updateCycleState_.refreshDirtyFlags());
          if (rootDefinition_.isSet() && !rootDefinition_->isBoundary())
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

        static void
        prepareRootBoundaryCompose(BoundaryNode *boundary, ComponentContext &rootContext, ComposeEvent event)
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
          if (director_.isUpdateCycleActive())
          {
            return false;
          }
          if (!beginPendingRefreshCycle())
          {
            clearPendingRefreshCycle();
            return false;
          }
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
          director_.beginApplyCycle();
          platformController_->beginApplyCycle();
          const NodeDirtyFlags flags = updateCycleState_.effectiveApplyDirtyFlags();
          logApplyFlags(flags);
          executePendingApplyCycle(flags);
          director_.completeUpdateCycle();
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
          : scene_(0),
            updateTransaction_(),
            activeTransaction_(),
            phase_(UPDATE_CYCLE_IDLE)
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

      inline bool SceneDirector::beginRefreshCycle()
      {
        if (phase_ != UPDATE_CYCLE_IDLE)
        {
          return false;
        }
        if (!updateTransaction_.hasAccumulatedUpdates())
        {
          return false;
        }
        activeTransaction_.swapTransaction(updateTransaction_);
        updateTransaction_.retainGeneration(activeTransaction_.generationValue());
        phase_ = UPDATE_CYCLE_REFRESH;
        return activeTransaction_.hasAccumulatedUpdates();
      }

      inline void SceneDirector::beginApplyCycle()
      {
        if (phase_ == UPDATE_CYCLE_REFRESH)
        {
          phase_ = UPDATE_CYCLE_APPLY;
        }
      }

      inline void SceneDirector::completeUpdateCycle()
      {
        activeTransaction_.clearTransaction();
        phase_ = UPDATE_CYCLE_IDLE;
        if (scene_ && updateTransaction_.hasAccumulatedUpdates())
        {
          scene_->queueInvalidate();
        }
      }

      inline SceneDirector::UpdateCyclePhase SceneDirector::phase() const
      {
        return phase_;
      }

      inline bool SceneDirector::isUpdateCycleActive() const
      {
        return phase_ != UPDATE_CYCLE_IDLE;
      }

      inline void
      SceneDirector::requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately)
      {
        if (!scene_)
        {
          return;
        }
        const BoundaryUpdateRequest request = normalizeBoundaryUpdateRequest(boundary, flags, flushImmediately);
        registerBoundaryUpdate(request);
        applyBoundaryUpdateRequest(request);
      }

      inline SceneDirector::BoundaryUpdateRequest SceneDirector::normalizeBoundaryUpdateRequest(
          BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately) const
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
        // New requests always accumulate into the pending transaction. When a
        // cycle is active, they become work for the next run.
        updateTransaction_.enqueueBoundaryUpdate(request);
      }

      inline void SceneDirector::applyBoundaryUpdateRequest(const BoundaryUpdateRequest &request) const
      {
        if (!scene_)
        {
          return;
        }
        if (isUpdateCycleActive())
        {
          return;
        }
        scene_->queueInvalidate();
        if (request.flushImmediately)
        {
          scene_->flushInvalidation();
        }
      }

      inline NodeDirtyFlags SceneDirector::aggregateDirtyFlags() const
      {
        return transactionForCurrentCycle().aggregateDirtyFlags();
      }

      inline NodeDirtyFlags SceneDirector::requestedDirtyFlags() const
      {
        return transactionForCurrentCycle().requestedDirtyFlags();
      }

      inline NodeDirtyFlags SceneDirector::effectiveRequestedDirtyFlags() const
      {
        return transactionForCurrentCycle().effectiveRequestedDirtyFlags();
      }

      inline bool SceneDirector::hasRequestedInput() const
      {
        return transactionForCurrentCycle().hasRequestedInput();
      }

      inline bool SceneDirector::requestedFullRebuild() const
      {
        return transactionForCurrentCycle().requestedFullRebuild();
      }

      inline NodeDirtyFlags SceneDirector::pendingDirtyFlagsForBoundary(const BoundaryNode *boundary) const
      {
        return transactionForCurrentCycle().pendingDirtyFlagsForBoundary(boundary);
      }

      inline bool SceneDirector::hasPendingBoundary(const BoundaryNode *boundary) const
      {
        return transactionForCurrentCycle().hasPendingBoundary(boundary);
      }

      inline BoundaryNode *SceneDirector::firstPendingBoundary() const
      {
        return transactionForCurrentCycle().firstPendingBoundary();
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
          if (parentFlags == NODE_DIRTY_CHILD && boundaryFlags != NODE_DIRTY_NONE)
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

      inline BoundaryNode *SceneDirector::rootBoundaryFor(Node *rootNode) const
      {
        return rootNode ? rootNode->asBoundary() : 0;
      }

      inline BoundaryNode *SceneDirector::primaryUpdateRootFor(Node *rootNode) const
      {
        BoundaryNode *root = firstPendingUpdateRoot();
        return root ? root : rootBoundaryFor(rootNode);
      }

      inline SceneDirector::SceneUpdateRequestSnapshot SceneDirector::buildRefreshRequestSnapshot(Node *rootNode) const
      {
        return activeTransaction_.buildRequestSnapshot(rootBoundaryFor(rootNode), primaryUpdateRootFor(rootNode));
      }

      inline SceneDirector::SceneUpdateSnapshot SceneDirector::buildUpdateSnapshot(Node *rootNode,
                                                                                   const Scene *scene) const
      {
        const unsigned long generation = activeTransaction_.snapshotGeneration();
        if (generation == 0)
        {
          return SceneUpdateSnapshot();
        }
        return finalizeUpdateSnapshot(generation, buildRefreshRequestSnapshot(rootNode), scene);
      }

      inline SceneDirector::SceneUpdateSnapshot SceneDirector::finalizeUpdateSnapshot(
          unsigned long generation, const SceneUpdateRequestSnapshot &request, const Scene *scene) const
      {
        SceneUpdateRequestSnapshot finalizedRequest = request;
        SceneUpdateApplySnapshot apply = buildApplySnapshot(scene);
        if (apply.layoutRequired())
        {
          finalizedRequest.includeDirtyFlags(NODE_DIRTY_LAYOUT);
        }
        SceneUpdateSnapshot snapshot(generation, finalizedRequest, apply);
        if (CanRelaxFullRebuildForLocalDiff(snapshot) || CanRelaxFullRebuildForChildOnlyUpdate(snapshot)
            || CanRelaxFullRebuildForRootBoundary(snapshot))
        {
          finalizedRequest.relaxFullRebuild();
        }
        return SceneUpdateSnapshot(generation, finalizedRequest, apply);
      }

      inline SceneDirector::SceneUpdateApplySnapshot SceneDirector::buildApplySnapshot(const Scene *scene) const
      {
        struct ApplySnapshotAccumulator
        {
          ApplySnapshotAccumulator()
              : structureRequired(false),
                layoutRequired(false),
                requiresCompositedPaint(false),
                hasOpaqueLocalPaint(true),
                canApplyLocalCompositionDiff(true),
                sawPaintWork(false),
                sawRoot(false)
          {
          }

          void observeRoot()
          {
            sawRoot = true;
          }

          void observeUpdateResult(const BoundaryUpdateResult &updateResult)
          {
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
          }

          void observeStructureRequirement(bool required)
          {
            if (required)
            {
              structureRequired = true;
            }
          }

          void observeComposeResult(const BoundaryComposeResult &composeResult)
          {
            if (!composeResult.composed || !composeResult.preservedNativeContexts)
            {
              canApplyLocalCompositionDiff = false;
            }
          }

          SceneUpdateApplySnapshot build() const
          {
            SceneUpdateApplySnapshot snapshot;
            bool opaqueLocalPaint = requiresCompositedPaint ? false : (sawPaintWork && hasOpaqueLocalPaint);
            bool localCompositionDiff = canApplyLocalCompositionDiff && sawRoot;
            snapshot.setRequirements(
                layoutRequired, structureRequired, requiresCompositedPaint, opaqueLocalPaint, localCompositionDiff);
            return snapshot;
          }

          bool structureRequired;
          bool layoutRequired;
          bool requiresCompositedPaint;
          bool hasOpaqueLocalPaint;
          bool canApplyLocalCompositionDiff;
          bool sawPaintWork;
          bool sawRoot;
        };

        struct RootStructureDecision
        {
          RootStructureDecision(const BoundaryComposeResult &composeResult,
                                const NodeCompositionDiff *diff,
                                NodeDirtyFlags effectiveDirtyFlags)
              : composed(composeResult.composed),
                hasDiff(diff != 0),
                diffEmpty(diff && diff->empty()),
                diffCompatibleRetainOnly(diff && diff->isCompatibleRetainOnly()),
                childDirty((effectiveDirtyFlags & NODE_DIRTY_CHILD) != 0),
                requiresStructure(false)
          {
            if (!composed || !hasDiff)
            {
              requiresStructure = !childDirty;
            }
            else if (!(childDirty && diffEmpty))
            {
              requiresStructure = !diffCompatibleRetainOnly;
            }
          }

          bool composed;
          bool hasDiff;
          bool diffEmpty;
          bool diffCompatibleRetainOnly;
          bool childDirty;
          bool requiresStructure;
        };

        ApplySnapshotAccumulator accumulator;
        PendingUpdateRootCursor updateRoots(this);
        BoundaryNode *root = updateRoots.next();
        while (root)
        {
          accumulator.observeRoot();
          const BoundaryUpdateResult &updateResult = root->updateResult();
          accumulator.observeUpdateResult(updateResult);

          const BoundaryComposeResult &composeResult = root->composeResult();
          const NodeCompositionDiff *diff = root->localCompositionDiff();
          const NodeDirtyFlags effectiveDirtyFlags =
              static_cast<NodeDirtyFlags>(pendingDirtyFlagsForBoundary(root) | composeResult.dirtyFlagsSeen);
          const RootStructureDecision structureDecision(composeResult, diff, effectiveDirtyFlags);
          const INestable *rootNestable = root->asNestable();
          const Node *firstChild = rootNestable ? rootNestable->childrenHead() : 0;
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneRootIdentity(
              static_cast<void *>(root->scene()),
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
                                                       structureDecision.hasDiff ? 1 : 0,
                                                       structureDecision.diffEmpty ? 1 : 0,
                                                       structureDecision.diffCompatibleRetainOnly ? 1 : 0,
                                                       structureDecision.requiresStructure ? 1 : 0);
#endif
            accumulator.observeStructureRequirement(structureDecision.requiresStructure);
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
                                                       structureDecision.requiresStructure ? 1 : 0);
#endif
            accumulator.observeStructureRequirement(structureDecision.requiresStructure);
          }
          else if (structureDecision.childDirty && structureDecision.diffEmpty)
          {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
            loka::platform::DebugLogSceneStructureRoot(static_cast<void *>(const_cast<Scene *>(scene)),
                                                       static_cast<void *>(root),
                                                       static_cast<unsigned int>(effectiveDirtyFlags),
                                                       1,
                                                       1,
                                                       1,
                                                       structureDecision.diffCompatibleRetainOnly ? 1 : 0,
                                                       structureDecision.requiresStructure ? 1 : 0);
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
                                                       structureDecision.diffEmpty ? 1 : 0,
                                                       structureDecision.diffCompatibleRetainOnly ? 1 : 0,
                                                       structureDecision.requiresStructure ? 1 : 0);
#endif
            accumulator.observeStructureRequirement(structureDecision.requiresStructure);
          }

          accumulator.observeComposeResult(composeResult);

          root = updateRoots.next();
        }
        return accumulator.build();
      }

      inline PlatformApplyPlan SceneDirector::buildPlatformApplyPlan(const SceneUpdateSnapshot &snapshot) const
      {
        PlatformApplyPlan plan;
        plan.structureChanged = snapshot.requiresStructureChange();
        plan.layoutChanged = snapshot.requiresLayoutChange();
        plan.paintKind = ResolvePaintKind(snapshot);
        plan.setPrimaryRoot(snapshot.request().primaryRoot());
        return plan;
      }

      inline PlatformApplyPlan SceneDirector::executeApplyPlan(Node *rootNode,
                                                               IPlatformController *platformController,
                                                               const SceneUpdateSnapshot &snapshot,
                                                               NodeDirtyFlags globalDirtyFlags,
                                                               bool fullRebuild) const
      {
        PlatformApplyPlan plan = buildPlatformApplyPlan(snapshot);
        applyPlatformApplyPlan(rootNode, platformController, plan, globalDirtyFlags, fullRebuild);
        return plan;
      }

      inline void SceneDirector::applyPlatformApplyPlan(Node *rootNode,
                                                        IPlatformController *platformController,
                                                        const PlatformApplyPlan &plan,
                                                        NodeDirtyFlags globalDirtyFlags,
                                                        bool fullRebuild) const
      {
        applyPendingBoundaryUpdates(rootNode, plan);
        if (shouldApplyGlobalChange(platformController, plan) && platformController)
        {
          platformController->onChange(rootNode, globalDirtyFlags, fullRebuild);
        }
      }

      inline void
      SceneDirector::applyPendingBoundaryUpdate(Node *rootNode, BoundaryNode *root, const PlatformApplyPlan &plan) const
      {
        assert(root && (isBoundaryUpdateRoot(root) || (root == rootBoundaryFor(rootNode) && !firstPendingUpdateRoot())));
        PlatformApplyPlan localPlan = plan.forBoundary(root);
        const BoundaryNode::LocalApplyInfo localInfo = root->localApplyInfo(localPlan);
        if (localPlan.hasBoundaryApplyWork(root) && root->scene() && root->scene()->platformController_)
        {
          root->scene()->platformController_->onBoundaryApply(rootNode, root, localInfo, localPlan);
        }
        root->applyPendingUpdate(localPlan);
      }

      inline void SceneDirector::applyPendingBoundaryUpdates(Node *rootNode, const PlatformApplyPlan &plan) const
      {
        PendingUpdateRootCursor updateRoots(this);
        BoundaryNode *root = updateRoots.next();
        if (!root)
        {
          root = rootBoundaryFor(rootNode);
        }
        while (root)
        {
          BoundaryApplyPhaseScope applyScope = root->beginApplyPhaseScope();
          applyPendingBoundaryUpdate(rootNode, root, plan);
          root = updateRoots.next();
        }
      }

      inline bool SceneDirector::shouldApplyGlobalChange(IPlatformController *platformController,
                                                         const PlatformApplyPlan &plan) const
      {
        return !platformController || !platformController->canSkipGlobalChangeForBoundaryLocalPaint()
               || !plan.isPaintOnlyWork();
      }

      inline void SceneDirector::SceneUpdateTransaction::clearTransaction()
      {
        clearAccumulatedState();
      }

      inline void SceneDirector::SceneUpdateTransaction::AccumulatedState::enqueueBoundaryUpdate(
          const BoundaryUpdateRequest &request)
      {
        if (!request.boundary)
        {
          return;
        }
        enqueueRequestedInput(request.flags);
        enqueueProjectionTarget(request.boundary, request.flags);
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

      inline SceneDirector::PendingUpdateRootAnalysis::PendingUpdateRootAnalysis(const SceneDirector *director)
          : director(director),
            seenHead(0),
            seenTail(0)
      {
      }

      inline SceneDirector::PendingUpdateRootAnalysis::~PendingUpdateRootAnalysis()
      {
        SeenRoot *entry = seenHead;
        while (entry)
        {
          SeenRoot *next = entry->next;
          delete entry;
          entry = next;
        }
        seenHead = 0;
        seenTail = 0;
      }

      inline bool SceneDirector::PendingUpdateRootAnalysis::hasEquivalentDescendant(BoundaryNode *root) const
      {
        if (!director || !root)
        {
          return false;
        }
        const NodeDirtyFlags rootFlags = director->pendingDirtyFlagsForBoundary(root);
        SceneProjectionTransaction::ConstIterator it =
            director->activeTransaction_.projectionTransaction().targetsBegin();
        while (it.isValid())
        {
          Node *node = it.node();
          BoundaryNode *boundary = node ? node->asBoundary() : 0;
          if (boundary != root && boundary && IsBoundaryDescendantOf(boundary, root) && it.dirtyFlags() == rootFlags)
          {
            const BoundaryComposeResult &candidateResult = boundary->composeResult();
            if (candidateResult.composed && candidateResult.preservedNativeContexts)
            {
              return true;
            }
          }
          it.next();
        }
        return false;
      }

      inline bool SceneDirector::PendingUpdateRootAnalysis::hasSeenRoot(BoundaryNode *root) const
      {
        if (!root)
        {
          return false;
        }
        SeenRoot *entry = seenHead;
        while (entry)
        {
          if (entry->root == root)
          {
            return true;
          }
          entry = entry->next;
        }
        return false;
      }

      inline void SceneDirector::PendingUpdateRootAnalysis::recordSeenRoot(BoundaryNode *root)
      {
        if (!root || hasSeenRoot(root))
        {
          return;
        }
        SeenRoot *entry = new SeenRoot();
        entry->root = root;
        if (!seenHead)
        {
          seenHead = entry;
          seenTail = entry;
        }
        else
        {
          seenTail->next = entry;
          seenTail = entry;
        }
      }

      inline bool SceneDirector::PendingUpdateRootAnalysis::shouldSkip(BoundaryNode *boundary, BoundaryNode *root) const
      {
        if (!director || !boundary || !root)
        {
          return true;
        }
        if (hasSeenRoot(root))
        {
          return true;
        }
        if ((director->pendingDirtyFlagsForBoundary(root) & NODE_DIRTY_CHILD) != 0 && hasEquivalentDescendant(root))
        {
          return true;
        }
        return false;
      }

      inline SceneDirector::PendingUpdateRootCursor::PendingUpdateRootCursor(const SceneDirector *director)
          : director(director),
            iterator(director ? director->activeTransaction_.projectionTransaction().targetsBegin()
                              : SceneProjectionTransaction::ConstIterator()),
            analysis(director)
      {
      }

      inline BoundaryNode *SceneDirector::PendingUpdateRootCursor::next()
      {
        while (iterator.isValid())
        {
          Node *node = iterator.node();
          iterator.next();

          BoundaryNode *boundary = node ? node->asBoundary() : 0;
          if (!boundary)
          {
            continue;
          }

          BoundaryNode *root = director ? director->topMostRequestedBoundary(boundary) : 0;
          if (root && !analysis.shouldSkip(boundary, root))
          {
            analysis.recordSeenRoot(root);
            return root;
          }
        }
        return 0;
      }

      inline static bool CanRelaxFullRebuildForLocalDiff(const SceneDirector::SceneUpdateSnapshot &snapshot)
      {
        return snapshot.request().effectiveFullRebuildRequired() && snapshot.apply().localCompositionDiffApplicable();
      }

      inline static bool CanRelaxFullRebuildForChildOnlyUpdate(const SceneDirector::SceneUpdateSnapshot &snapshot)
      {
        return snapshot.request().effectiveFullRebuildRequired()
               && snapshot.request().hasEffectiveDirtyFlag(NODE_DIRTY_CHILD) && !snapshot.apply().structureRequired();
      }

      inline static bool CanRelaxFullRebuildForRootBoundary(const SceneDirector::SceneUpdateSnapshot &snapshot)
      {
        if (!snapshot.request().effectiveFullRebuildRequired())
        {
          return false;
        }
        BoundaryNode *rootBoundary = snapshot.request().rootBoundaryValue();
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
        bool started = afterRoot == 0;
        PendingUpdateRootCursor cursor(this);
        BoundaryNode *root = cursor.next();
        while (root)
        {
          if (started)
          {
            return root;
          }
          started = root == afterRoot;
          root = cursor.next();
        }
        return 0;
      }

      inline void SceneDirector::clearUpdateTransaction()
      {
        updateTransaction_.clearTransaction();
        activeTransaction_.clearTransaction();
        phase_ = UPDATE_CYCLE_IDLE;
      }

      inline void SceneDirector::SceneUpdateTransaction::enqueueBoundaryUpdate(const BoundaryUpdateRequest &request)
      {
        accumulatedState.enqueueBoundaryUpdate(request);
      }

      inline const SceneDirector::SceneUpdateTransaction &SceneDirector::transactionForCurrentCycle() const
      {
        return isUpdateCycleActive() ? activeTransaction_ : updateTransaction_;
      }

      inline SceneDirector::SceneUpdateTransaction &SceneDirector::transactionForCurrentCycle()
      {
        return isUpdateCycleActive() ? activeTransaction_ : updateTransaction_;
      }

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENE_HPP
