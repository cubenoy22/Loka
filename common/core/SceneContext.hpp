#ifndef DECLARA_SCENECONTEXT_HPP
#define DECLARA_SCENECONTEXT_HPP

#include "core/Scene.hpp"

// 先行宣言
class SceneNode;
class SceneNodeContext;

// 抽象: プラットフォームごとのリソース・ライフサイクル管理
class SceneContext
{
public:
  virtual ~SceneContext() {}

  // --- SceneNodeContext生成・再取得API ---
  // メモリ管理はPlatformContext側に委譲
  virtual SceneNodeContext *getNodeContext(SceneNode *node) = 0;
  // 必要に応じて: シングルトン/都度生成の切替は実装側で
  // ライフサイクルStateのbindも今後拡張予定
};

#endif // DECLARA_SCENECONTEXT_HPP
