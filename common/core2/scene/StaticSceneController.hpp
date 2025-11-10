#ifndef DECLARA_CORE2_SCENE_STATICSCENECONTROLLER_HPP_INCLUDED
#define DECLARA_CORE2_SCENE_STATICSCENECONTROLLER_HPP_INCLUDED

#include "core2/scene/Controller.hpp"
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/NodeComposition.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class Node; // 前方宣言

      // StaticSceneController: Solid.js型の静的ツリーを構築し、PlatformControllerに渡す
      class StaticSceneController : public declara::core::scene::Controller
      {
      private:
        IPlatformController *platformController_;
        Node *rootNode_;

      public:
        explicit StaticSceneController(Scene *scene, IPlatformController *platformController)
            : Controller(scene), platformController_(platformController), rootNode_(0)
        {
        }

        virtual ~StaticSceneController()
        {
          if (platformController_)
          {
            platformController_->destroy();
          }
          delete rootNode_; // Nodeツリーの解放
        }

        // UIの構築と実行を開始する
        void run()
        {
          if (scene_ && platformController_)
          {
            // 1. composeを呼び出してNodeDefinitionツリーを構築
            NodeComposition composition;
            scene_->compose(composition);

            // 2. NodeDefinitionツリーから永続的なNodeツリーを生成
            //    (NodeCompositionにcreateNodeTree()のようなメソッドがあると仮定)
            // rootNode_ = composition.createNodeTree();

            // 3. PlatformControllerにNodeツリーを渡し、UIの初回生成を依頼
            // platformController_->materialize(rootNode_);
          }
        }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_STATICSCENECONTROLLER_HPP_INCLUDED
