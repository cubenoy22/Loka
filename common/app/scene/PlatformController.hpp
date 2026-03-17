#ifndef LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP
#define LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {

      class Node;
      enum NodeDirtyFlags;

      // プラットフォーム固有コントローラーの抽象インターフェース
      // UIの同期 (Synchronization) を担当する
      class IPlatformController
      {
      public:
        virtual ~IPlatformController() {}

        // Nodeツリーを受け取り、UIに反映する（flagsは将来の差分用）
        virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild) = 0;

        // 変更があったNodeをUIに同期する
        virtual void synchronize() = 0;
        virtual bool hasPendingSync() const = 0;

        // UIリソースを破棄する
        virtual void destroy() = 0;

        // Retired subtree cleanup without forcing a full scene rebuild.
        virtual void releaseNodeContexts(Node *) {}
      };

    }
  }
}

#endif // LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP
