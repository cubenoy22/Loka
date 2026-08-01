#ifndef LOKA_CORE2_SCENE_PROJECTION_RETAINED_NODE_HANDLER_HPP
#define LOKA_CORE2_SCENE_PROJECTION_RETAINED_NODE_HANDLER_HPP

#include "app/scene/projection/PlatformNodeHandler.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Shared retained-context installation wall. setContext must precede
          readLifecycleFactOnAttach, and afterAttach must remain last. In
          particular, Boundary compose may already have consumed pendingAttach
          before an OpenFileDialog context is created, so its afterAttach hook
          must present after the lifecycle fact has been read. The hook remains
          creation-only so an existing dialog does not present twice. */
      template <typename Derived, typename NodeT, typename CtxT>
      class RetainedNodeHandler : public IPlatformNodeHandler
      {
      public:
        virtual const void *nodeTypeKey() const
        {
          return NodeTypeToken<NodeT>();
        }

        virtual NodeContext *ensureContext(Node *node,
                                           IPlatformController *controller,
                                           const LayoutState &state)
        {
          NodeT *typed = Derived::cast(node);
          if (!typed || !controller)
          {
            return 0;
          }

          CtxT *ctx = static_cast<CtxT *>(typed->getContext());
          if (ctx)
          {
            Derived::refresh(ctx, state);
            return ctx;
          }

          ctx = Derived::create(typed, controller, state);
          if (!ctx)
          {
            return 0;
          }
          typed->setContext(ctx);
          ctx->readLifecycleFactOnAttach();
          Derived::afterAttach(ctx);
          return ctx;
        }

        static void afterAttach(CtxT *)
        {
        }

        static void refresh(CtxT *, const LayoutState &)
        {
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_RETAINED_NODE_HANDLER_HPP
