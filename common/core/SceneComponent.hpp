#ifndef DECLARA_SCENECOMPONENT_HPP
#define DECLARA_SCENECOMPONENT_HPP

// --- SceneComponent: SceneBuilder配下の抽象基底 ---
class SceneComponent
{
public:
  virtual ~SceneComponent() {}
  // 差分検知・再描画APIなど拡張予定
};

#endif // DECLARA_SCENECOMPONENT_HPP
