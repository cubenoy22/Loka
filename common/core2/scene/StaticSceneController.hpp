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

      public:
        explicit StaticSceneController(Scene *scene, IPlatformController *platformController)
            : Controller(scene), platformController_(platformController)
        {
        }

        virtual ~StaticSceneController()
        {
          if (platformController_)
          {
            platformController_->destroy();
            platformController_ = 0;
          }
        }

        // UIの構築と実行を開始する
        void run()
        {
          if (!scene_ || !platformController_)
          {
            return;
          }

          compose();

          Node *rootNode = root();
          if (rootNode)
          {
            platformController_->materialize(rootNode);
          }
        }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_STATICSCENECONTROLLER_HPP_INCLUDED
