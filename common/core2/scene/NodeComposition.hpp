#ifndef DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP

namespace declara {
namespace core {
namespace scene {

struct NodeComposition {
  // UIツリーのルートノードを保持
  void* root; // 仮: 実際はSceneNodeGroup*やNode*等に差し替え予定

  template<typename T>
  T& declare(T& x) {
    root = &x;
    return x;
  }
};

} // namespace scene
} // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
