#ifndef DECLARA_CORE2_SCENE_CONTROLLER_HPP_INCLUDED
#define DECLARA_CORE2_SCENE_CONTROLLER_HPP_INCLUDED

#include "core2/scene/Scene.hpp"
#include "core2/scene/Node.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {

      // Controller: SceneController的な役割にリファクタ
      // - Sceneの管理・compose戦略の切り替え・ノードツリーの所有/解放を担う
      // - StaticSceneController（Solid.js型）やDynamicSceneController（React型）など戦略ごとに派生クラスで実装
      // - Sceneは宣言的UI定義に専念

      class Controller
      {
      public:
        // Sceneの所有権は呼び出し元で管理
        explicit Controller(declara::core::scene::Scene *scene) : scene_(scene), rootNode_(0)
        {
          if (scene_)
          {
            State<SceneLifecycle> *lifecycle = scene_->getLifecycleState();
            if (lifecycle)
            {
              lifecycle->deferBindWithOld(&Controller::onLifecycleChanged, this);
            }
          }
        }
        virtual ~Controller() { clear(); }

        // compose戦略ごとにオーバーライド
        virtual void compose()
        {
          if (!scene_)
          {
            return;
          }
          clear();
          declara::core::scene::NodeComposition composition;
          scene_->compose(composition);
          rootNode_ = composition.createNodeTree();
        }

        // ノードツリーのルート取得
        declara::core::scene::Node *root() const { return rootNode_; }

        // ノードツリーの解放
        virtual void clear()
        {
          if (rootNode_)
          {
            delete rootNode_;
            rootNode_ = 0;
          }
        }

      protected:
        declara::core::scene::Scene *scene_;
        declara::core::scene::Node *rootNode_;

      private:
        static void onLifecycleChanged(SceneLifecycle newValue, SceneLifecycle oldValue, void *userData)
        {
          Controller *self = (Controller *)userData;
          if (self && newValue == ON_ATTACH)
          {
            self->compose();
          }
        }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_CONTROLLER_HPP_INCLUDED
