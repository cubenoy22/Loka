#ifndef DECLARA_CORE2_SCENE_SCENE_HPP
#define DECLARA_CORE2_SCENE_SCENE_HPP

#include "core2/scene/NodeComposition.hpp"

namespace declara {
namespace core {
namespace scene {

class Scene {
public:
  // 今後拡張予定の宣言的UIシーン基底クラス
  Scene() {}
  virtual ~Scene() {}
  virtual void compose(NodeComposition c) {}
};

} // namespace scene
} // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_SCENE_HPP
