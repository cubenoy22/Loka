#ifndef LOKA_CORE2_SCENE_PROJECTION_IPLATFORMCONTROLLER_HPP
#define LOKA_CORE2_SCENE_PROJECTION_IPLATFORMCONTROLLER_HPP

#include "app/scene/Node.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "app/scene/boundary/BoundaryApplyInfo.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {

      class Node;
      class BoundaryNode;
      enum NodeDirtyFlags;
      struct PlatformApplyPlan;

      class IPlatformController;
      class OperationScope;
      class OperationPhase;
    }
#ifdef TEST_BUILD
    namespace testing
    {
      scene::OperationPhase &controllerlessOperationPhase();
    }
#endif
    namespace scene
    {
      /** Controller-owned operation state, independent of Scene run/focus phases. */
      class OperationPhase
      {
        friend class IPlatformController;
        friend class OperationScope;
#ifdef TEST_BUILD
        friend OperationPhase &loka::app::testing::controllerlessOperationPhase();
#endif
      public:
        bool open() const { return this->open_; }
      private:
        OperationPhase() : open_(false) {}
        OperationPhase(const OperationPhase &);
        OperationPhase &operator=(const OperationPhase &);
        bool open_;
      };
      /** Stack borrow interval. Only this writer opens/restores the phase. */
      class OperationScope
      {
      public:
        explicit OperationScope(IPlatformController &controller);
#ifdef TEST_BUILD
        explicit OperationScope(OperationPhase &phase)
            : phase_(phase), previous_(phase.open_)
        {
          this->phase_.open_ = true;
        }
#endif
        ~OperationScope() { this->phase_.open_ = this->previous_; }
      private:
        OperationScope(const OperationScope &);
        OperationScope &operator=(const OperationScope &);
        OperationPhase &phase_;
        const bool previous_;
      };

      /**
       * Abstract platform controller for projecting scene changes into native UI.
       */
      class IPlatformController
      {
        friend class OperationScope;
        OperationPhase operationPhase_;
        IPlatformController(const IPlatformController &);
        IPlatformController &operator=(const IPlatformController &);
      public:
        IPlatformController() {}
        virtual ~IPlatformController() { assert(!this->operationPhase_.open()); }
        const OperationPhase &operationPhase() const { return this->operationPhase_; }

        /** False cannot answer; true with null means native focus is absent. */
        virtual bool readNativeFocus(NodeContext *&out)
        {
          (void)out;
          return false;
        }

        /** Attempts native focus without activating the window. Copy native handles
            and the context identity BEFORE any native call. Callbacks may retire
            and free the context: afterwards never touch ctx, only compare identities.
            Completion re-reads native focus; the return is not publication authority. */
        virtual bool applyNativeFocus(NodeContext &ctx)
        {
          (void)ctx;
          return false;
        }

        // Project a changed node tree into native UI.
        virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild) = 0;

        // Optional boundary-local apply seam. Default is no-op to preserve existing controllers.
        virtual void onBoundaryApply(Node *, BoundaryNode *, const BoundaryLocalApplyInfo &, const PlatformApplyPlan &)
        {
        }

        // Platforms that can fully consume pure paint-only updates through onBoundaryApply()
        // may opt into skipping the legacy global onChange() callback for those cycles.
        virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const
        {
          return false;
        }

        // Optional hook to start a new scene apply/update measurement cycle.
        virtual void beginApplyCycle() {}

        // Synchronize pending native UI changes.
        virtual void synchronize() = 0;
        virtual bool hasPendingSync() const = 0;

        /** Destroys native handles queued by terminal context delivery. This
            runs only at the platform safe point, after native callbacks have
            unwound and immediately before the App reclaim boundary. */
        virtual void drainNativeRetirements()
        {
#ifdef LOKA_LIFECYCLE_AUDIT
          assert(!this->operationPhase().open());
#endif
        }

        // Destroy platform-owned UI resources.
        virtual void destroy() = 0;

        /** Completes the detach line for every context owned by a retired
            subtree without forcing a full scene rebuild. Terminal fact
            delivery must hide, unbind, remove native event routes, and queue
            native destruction before the context object is deleted here;
            actual native destruction is forbidden on this path. */
        virtual void releaseNodeContexts(Node *node)
        {
#ifdef LOKA_LIFECYCLE_AUDIT
          assert(!this->operationPhase().open());
#endif
          if (!node)
          {
            return;
          }
          // Parked retained branches (Conditional slots) hand their native
          // pairs over here too — the retire door, not the reclaim drain,
          // is where every context in the subtree ends.
          for (unsigned i = 0; Node *branch = node->retainedLifecycleBranch(i); ++i)
          {
            IPlatformController::releaseNodeContexts(branch);
          }
          INestable *nestable = node->asNestable();
          if (nestable)
          {
            for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
            {
              IPlatformController::releaseNodeContexts(child);
            }
          }
          node->setContext(0);
        }

        // Generic hook for nodes that can begin their own platform projection.
        virtual bool prepareProjectedLayout(Node *, LayoutState &)
        {
          return false;
        }

        // Optional external extension seam for platform-specific/custom node handlers.
        virtual bool registerNodeHandler(IPlatformNodeHandler *)
        {
          return false;
        }
      };

      inline OperationScope::OperationScope(IPlatformController &controller)
          : phase_(controller.operationPhase_), previous_(this->phase_.open_)
      {
        this->phase_.open_ = true;
      }
    } // namespace scene
#ifdef TEST_BUILD
    namespace testing
    {
      /** Scene-less fixtures only; no controller or Scene consumes this phase. */
      inline scene::OperationPhase &controllerlessOperationPhase()
      {
        static scene::OperationPhase phase;
        return phase;
      }
    }
#endif
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_IPLATFORMCONTROLLER_HPP
