#ifndef DECLARA_SCENECONTEXT_HPP
#define DECLARA_SCENECONTEXT_HPP

#include "core/Scene.hpp"

// 抽象: プラットフォームごとのリソース・ライフサイクル管理
class SceneContext
{
public:
  virtual ~SceneContext() {}
  // 必要に応じて拡張（リソース管理API等）
};

#endif // DECLARA_SCENECONTEXT_HPP
