#ifndef LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP
#define LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP

#include "core2/scene/Node.hpp"

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
        virtual void onChange(Node *rootNode, NodeDirtyFlags flags) = 0;

        // 変更があったNodeをUIに同期する
        virtual void synchronize() = 0;

        // UIリソースを破棄する
        virtual void destroy() = 0;
      };

    }
  }
}

#endif // LOKA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP
