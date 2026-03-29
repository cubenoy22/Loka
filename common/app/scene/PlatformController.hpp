#ifndef LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP
#define LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP

#include "app/scene/Node.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include "app/scene/node/BoundaryApplyInfo.hpp"

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

      // プラットフォーム固有コントローラーの抽象インターフェース
      // UIの同期 (Synchronization) を担当する
      class IPlatformController
      {
      public:
        virtual ~IPlatformController() {}

        // Nodeツリーを受け取り、UIに反映する（flagsは将来の差分用）
        virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild) = 0;

        // Optional boundary-local apply seam. Default is no-op to preserve existing controllers.
        virtual void onBoundaryApply(Node *, BoundaryNode *, const BoundaryLocalApplyInfo &, const PlatformApplyPlan &) {}

        // Platforms that can fully consume pure paint-only updates through onBoundaryApply()
        // may opt into skipping the legacy global onChange() callback for those cycles.
        virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const { return false; }

        // Optional hook to start a new scene apply/update measurement cycle.
        virtual void beginApplyCycle() {}

        // 変更があったNodeをUIに同期する
        virtual void synchronize() = 0;
        virtual bool hasPendingSync() const = 0;

        // UIリソースを破棄する
        virtual void destroy() = 0;

        // Retired subtree cleanup without forcing a full scene rebuild.
        virtual void releaseNodeContexts(Node *) {}

        // Generic hook for nodes that can begin their own platform projection.
        virtual bool prepareProjectedLayout(Node *, LayoutState &) { return false; }

        // Optional external extension seam for platform-specific/custom node handlers.
        virtual bool registerNodeHandler(IPlatformNodeHandler *) { return false; }
      };

    }
  }
}

#endif // LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP
