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

      /**
       * Abstract platform controller for projecting scene changes into native UI.
       */
      class IPlatformController
      {
      public:
        virtual ~IPlatformController() {}

        /** Refusal-only: O(1) input restoration and dispatch, no rows walked
            here. Include this attempt's inputs and request an after-flush layout;
            requestRelayout retains the rail's scheduling/bookkeeping costs. */
        void refuseTextMeasurement(Node *node, const LayoutState &state)
        {
          if (!node)
            return;
          node->requeueLayoutInputs(state.inputs);
          this->requestRelayout();
        }

        /** Schedule layout after the current flush; never perform it inline.
            Controllers without layout have no work to schedule. */
        virtual void requestRelayout() {}

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
        virtual void drainNativeRetirements() {}

        // Destroy platform-owned UI resources.
        virtual void destroy() = 0;

        /** Completes the detach line for every context owned by a retired
            subtree without forcing a full scene rebuild. Terminal fact
            delivery must hide, unbind, remove native event routes, and queue
            native destruction before the context object is deleted here;
            actual native destruction is forbidden on this path. */
        virtual void releaseNodeContexts(Node *node)
        {
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

      protected:
        /** Route through the controller's mounted Scene root without a tree walk.
            Null, Toolbox and Win32 share this after-run scheduling mechanism. */
        static void requestSceneRelayout(Node *rootNode);
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_IPLATFORMCONTROLLER_HPP
