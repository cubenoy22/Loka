#ifndef DECLARA_CORE2_SCENE_STATICSCENECONTROLLER_HPP_INCLUDED
#define DECLARA_CORE2_SCENE_STATICSCENECONTROLLER_HPP_INCLUDED

#include "core2/scene/Controller.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {

      // StaticSceneController: Solid.js型の即時反映・静的ノードツリー生成
      class StaticSceneController : public declara::core::scene::Controller
      {
      public:
        explicit StaticSceneController(Scene *scene) : Controller(scene) {}
        virtual ~StaticSceneController() { clear(); }

        // compose: 毎回ノードツリーを新規生成しrootNode_にセット
        virtual void compose()
        {
          clear();
          if (scene_)
          {
            NodeComposition c;
            scene_->compose(c);
            // rootNode_ = c.releaseRoot(); // NodeCompositionからrootノードを取得
          }
        }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_STATICSCENECONTROLLER_HPP_INCLUDED
