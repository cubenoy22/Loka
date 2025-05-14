#ifndef DECLARA_TREEDSCENECOMPONENT_HPP
#define DECLARA_TREEDSCENECOMPONENT_HPP

#include "core/SceneComponent.hpp"
#include <vector>

// --- TreedSceneComponentLeaf: Solid.js的な最小再描画単位 ---
class TreedSceneComponentLeaf : public SceneComponent
{
public:
  virtual ~TreedSceneComponentLeaf() {}
  // 差分検知・再描画APIなど拡張予定
};

// --- TreedSceneComponent: ツリー型SceneComponent ---
class TreedSceneComponent : public SceneComponent
{
public:
  TreedSceneComponent() {}
  virtual ~TreedSceneComponent()
  {
    for (size_t i = 0; i < children.size(); ++i)
      delete children[i];
  }
  void addChild(SceneComponent *child)
  {
    children.push_back(child);
  }
  const std::vector<SceneComponent *> &getChildren() const { return children; }

  // Abstract: must be implemented by concrete subclasses
  virtual void update() = 0;

protected:
  std::vector<SceneComponent *> children;
};

#endif // DECLARA_TREEDSCENECOMPONENT_HPP
