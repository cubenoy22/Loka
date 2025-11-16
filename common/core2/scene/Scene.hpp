#ifndef DECLARA_CORE2_SCENE_SCENE_HPP
#define DECLARA_CORE2_SCENE_SCENE_HPP

#include "core2/scene/NodeComposition.hpp"
#include "core/State.hpp"

enum SceneLifecycle
{
  ON_CREATE = 0,
  ON_ATTACH = 1,
  ON_DETACH = 2
};

namespace declara
{
  namespace core
  {
    namespace scene
    {

      class Scene
      {
      public:
        Scene() : lifecycle_(ON_CREATE) {}
        virtual ~Scene() {}
        virtual void compose(NodeComposition &c) {}

        // 外部公開: State*として取得
        State<SceneLifecycle> *getLifecycleState() { return &lifecycle_; }

      protected:
        MutableState<SceneLifecycle> lifecycle_;

        // SceneManager2からlifecycle_を書き換え可能に
        friend class SceneManager2;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_SCENE_HPP
