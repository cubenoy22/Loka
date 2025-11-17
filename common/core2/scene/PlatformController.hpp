#ifndef DECLARA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP
#define DECLARA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP

namespace declara
{
  namespace core
  {
    namespace scene
    {

      class Node;

      // プラットフォーム固有コントローラーの抽象インターフェース
      // UIの同期 (Synchronization) を担当する
      class IPlatformController
      {
      public:
        virtual ~IPlatformController() {}

        // Nodeツリーを受け取り、UIを初回生成(具現化)する
        virtual void materialize(Node *rootNode) = 0;

        // 変更があったNodeをUIに同期する
        virtual void synchronize() = 0;

        // UIリソースを破棄する
        virtual void destroy() = 0;
      };

    }
  }
}

#endif // DECLARA_CORE2_SCENE_IPLATFORMCONTROLLER_HPP
